#!/usr/bin/env python3
"""Verify targeted ``.KILL`` and ``.X KILL`` GM commands."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from modes.gm_kill import ACCOUNT, PASSWORD, TARGET_SERIALS
from run_suite import shutdown_failures, stop_server, tail, wait_for_port


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _talk(text: str) -> bytes:
    encoded = text.encode("ascii") + b"\0"
    return struct.pack(">BHBHH", 0x03, 8 + len(encoded), 0, 0, 3) + encoded


def _make_target(context: int, uid: int) -> bytes:
    packet = bytearray(19)
    packet[0] = 0x6C
    packet[2:6] = context.to_bytes(4, "big")
    packet[7:11] = uid.to_bytes(4, "big")
    return bytes(packet)


def _drain(sock: socket.socket, timeout: float = 1.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
    finally:
        sock.setblocking(True)
    return bytes(data)


def _recv_until(sock: socket.socket, predicate, timeout: float = 5.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
            if predicate(_decode(bytes(data))):
                return bytes(data)
    finally:
        sock.setblocking(True)
    return bytes(data)


def _target_context(raw_packets) -> int | None:
    for packet in raw_packets:
        if packet.command != 0x6C or len(packet.data) < 19:
            continue
        context = int.from_bytes(packet.data[2:6], "big")
        if context:
            return context
    return None


def _has_death_or_corpse(raw_packets, serial: int) -> bool:
    for packet in raw_packets:
        if packet.command == 0xAF and len(packet.data) >= 5:
            if int.from_bytes(packet.data[1:5], "big") & 0x7FFFFFFF == serial:
                return True
        if packet.command == 0x1D and len(packet.data) >= 5:
            if int.from_bytes(packet.data[1:5], "big") & 0x7FFFFFFF == serial:
                return True
        if packet.command == 0x1A and len(packet.data) >= 5:
            # A corpse packet is sufficient evidence after the target's death.
            return True
    return False


def _enter(port: int) -> socket.socket:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect("127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("GM kill fixture did not reach the character list")
    sock.sendall(make_char_play(0))
    if not recv_until_game_start(sock, timeout=20.0):
        sock.close()
        raise RuntimeError("GM kill fixture character did not enter the world")
    _drain(sock, 0.5)
    return sock


def _run_command(sock: socket.socket, command: str, serial: int) -> tuple[bool, bool]:
    sock.sendall(_talk(f".{command}"))
    target_data = _recv_until(sock, lambda packets: _target_context(packets) is not None)
    context = _target_context(_decode(target_data))
    if context is None:
        return False, False
    sock.sendall(_make_target(context, serial))
    result = _recv_until(
        sock,
        lambda packets: _has_death_or_corpse(packets, serial),
        timeout=5.0,
    )
    return True, _has_death_or_corpse(_decode(result), serial)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2950)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    process = None
    returncode = None
    try:
        with (fixture / "server.log").open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, args.startup_timeout)
            sock = _enter(args.port)
            try:
                for command, serial in zip(("KILL", "X KILL"), TARGET_SERIALS):
                    cursor, killed = _run_command(sock, command, serial)
                    if not cursor:
                        failures.append(f".{command} did not open a target cursor")
                    elif not killed:
                        failures.append(f".{command} produced no death, removal, or corpse packet")
            finally:
                sock.close()
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        contents = (fixture / "server.log").read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, contents))
    for command in ("KILL", "X KILL"):
        if f"command '{command}.' error" in contents:
            failures.append(f"server retained the trailing-dot command for .{command}")

    if failures:
        print("GM kill probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(fixture / "server.log"), file=sys.stderr)
        return 1
    print("GM kill probe passed: .KILL and .X KILL targeted animal commands")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
