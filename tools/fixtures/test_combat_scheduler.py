#!/usr/bin/env python3
"""Verify that a player keeps scheduling melee swings against a static target."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from modes.combat_scheduler import ACCOUNT, PASSWORD, TARGET_SERIAL
from run_suite import shutdown_failures, stop_server, wait_for_port


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _recv(sock: socket.socket, timeout: float) -> list:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
    return _decode(bytes(data))


def _enter(port: int) -> socket.socket:
    import uo_test_client as uo
    sock, _ = uo.game_connect("127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("combat scheduler fixture did not reach the character list")
    sock.sendall(uo.make_char_play(0))
    start_data = uo.recv_until_game_start(sock, timeout=20.0)
    if not start_data:
        sock.close()
        raise RuntimeError("combat scheduler fixture character did not enter the world")
    # Parse the entry stream so malformed framing fails before the attack loop.
    uo.split_packet_stream(uo.decode_game_response(start_data), allow_truncated=True)
    _recv(sock, 0.5)
    return sock


def _war() -> bytes:
    return bytes((0x72, 1, 0, 0x32, 0))


def _attack(serial: int) -> bytes:
    return struct.pack(">BI", 0x05, serial)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2952)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--seconds", type=float, default=20.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    result: dict[str, object] = {}
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
            sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
            sock = _enter(args.port)
            try:
                sock.sendall(_war())
                _recv(sock, 1.0)
                started = time.monotonic()
                sent = 0
                received = []
                while time.monotonic() - started < args.seconds:
                    sock.sendall(_attack(TARGET_SERIAL))
                    sent += 1
                    received.extend(_recv(sock, 1.0))
                combat_texts = []
                for packet in received:
                    if packet.command != 0x1C or len(packet.data) < 44:
                        continue
                    text = packet.data[44:].split(b"\0", 1)[0].decode("latin-1", errors="replace")
                    if "You hit" in text or "You Miss" in text:
                        combat_texts.append(text)
                result["attack_requests"] = sent
                result["animations"] = sum(packet.command == 0x2F for packet in received)
                result["combat_results"] = len(combat_texts)
                result["target_removals"] = sum(
                    packet.command == 0x1D
                    and len(packet.data) >= 5
                    and (struct.unpack_from(">I", packet.data, 1)[0] & 0x7FFFFFFF) == TARGET_SERIAL
                    for packet in received
                )
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
        log_contents = (fixture / "server.log").read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, log_contents))
    if result.get("combat_results", 0) < 2:
        failures.append(
            f"melee scheduler produced fewer than two hit/miss results: {result!r}"
        )
    if failures:
        print("combat scheduler probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print(
        "combat scheduler probe passed: "
        f"{result['combat_results']} hit/miss results from {result['attack_requests']} requests"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
