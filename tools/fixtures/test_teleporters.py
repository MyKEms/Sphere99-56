#!/usr/bin/env python3
"""Verify dynamic and map teleporter arrival with bounded packet probes."""

from __future__ import annotations

import argparse
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, tail, wait_for_port


ACCOUNT = "FixturePlayer"
PASSWORD = "fixture-pw"
EXPECTED_DYNAMIC = (128, 127, 0)
EXPECTED_STATIC = (128, 129, 0)
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _talk(command: str) -> bytes:
    encoded = command.encode("ascii") + b"\0"
    return struct.pack(">BHBHH", 0x03, 8 + len(encoded), 0, 0, 3) + encoded


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


def _system_text(packet) -> str:
    if packet.command != 0x1C or len(packet.data) < 45:
        return ""
    return packet.data[44:].split(b"\0", 1)[0].decode("latin1", errors="replace")


def _where(sock: socket.socket) -> tuple[int, int, int] | None:
    sock.sendall(_talk("/WHERE"))
    response = _drain(sock, 1.5)
    for packet in reversed(_decode(response)):
        match = re.search(r"\((-?\d+),(-?\d+),(-?\d+)\)", _system_text(packet))
        if match:
            return tuple(int(value) for value in match.groups())
    return None


def _walk(sock: socket.socket, direction: int, sequence: int) -> bytes:
    sock.sendall(struct.pack(">BBBI", 0x02, direction, sequence & 0xFF, 0))
    return _recv_until(
        sock,
        lambda packets: any(packet.command == 0x22 for packet in packets),
        timeout=3.0,
    )


def _enter(port: int) -> socket.socket:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import decode_game_response, find_start_packet, game_relogin, make_char_play, recv_until_game_start

    sock, _, initial = game_relogin(
        "127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000
    )
    if sock is None:
        raise RuntimeError("teleporter fixture did not reach the character list")
    if find_start_packet(initial) is None:
        sock.sendall(make_char_play(0))
        initial += decode_game_response(recv_until_game_start(sock, timeout=10.0))
    if find_start_packet(decode_game_response(initial)) is None:
        sock.close()
        raise RuntimeError("teleporter fixture character did not enter the world")
    _drain(sock, 0.3)
    return sock


def _goto(sock: socket.socket, x: int, y: int, z: int = 0) -> None:
    sock.sendall(_talk(f"/GO {x},{y},{z}"))
    _drain(sock, 1.0)


def run_probe(port: int) -> list[str]:
    failures: list[str] = []
    sock = None
    try:
        sock = _enter(port)
        start = _where(sock)
        if start != (128, 128, 0):
            failures.append(f"initial position was {start!r}, expected (128, 128, 0)")

        # The first packet turns east; the second steps onto the dynamic pad.
        _walk(sock, 2, 1)
        _walk(sock, 2, 2)
        dynamic = _where(sock)
        if dynamic != EXPECTED_DYNAMIC:
            failures.append(f"dynamic teleporter position was {dynamic!r}, expected {EXPECTED_DYNAMIC!r}")

        _goto(sock, 128, 130, 0)
        _walk(sock, 2, 3)
        static = _where(sock)
        if static != EXPECTED_STATIC:
            failures.append(f"map teleporter position was {static!r}, expected {EXPECTED_STATIC!r}")
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        failures.append(str(error))
    finally:
        if sock is not None:
            sock.close()
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2801)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    log_path = fixture / "server.log"
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, 120.0)
            failures.extend(run_probe(args.port))
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, log_contents))
    failures.extend(
        f"server log contains sanitizer output: {line}"
        for line in log_contents.splitlines()
        if any(marker in line for marker in SANITIZER_MARKERS)
    )
    if failures:
        print("teleporter probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1
    print("teleporter probe passed: dynamic and map teleporters reached their destinations")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
