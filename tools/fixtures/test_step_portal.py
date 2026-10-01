#!/usr/bin/env python3
"""Check that an item's @Step trigger gates a moongate before default teleport."""

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


ACCOUNT = "StepPortalProbe"
ACCOUNT_KEY = "step-portal-pw"
EXPECTED_START = (128, 128, 0)
PORTAL_DESTINATION = (130, 130, 0)
QUALIFIED_START = (128, 130, 0)
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


def _enter(port: int, slot: int) -> socket.socket:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect("127.0.0.1", port, ACCOUNT, ACCOUNT_KEY, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"step portal fixture did not reach character list for slot {slot}")
    sock.sendall(make_char_play(slot))
    if not recv_until_game_start(sock, timeout=10.0):
        sock.close()
        raise RuntimeError(f"step portal fixture slot {slot} did not enter the world")
    _drain(sock, 0.3)
    return sock


def _step(sock: socket.socket, sequence: int) -> tuple[tuple[int, int, int] | None, list[str]]:
    _walk(sock, 2, sequence - 1)  # turn east
    response = _walk(sock, 2, sequence)
    return _where(sock), [_system_text(packet) for packet in _decode(response) if _system_text(packet)]


def run_probe(port: int) -> list[str]:
    failures: list[str] = []
    sock = None
    try:
        sock = _enter(port, 0)
        start = _where(sock)
        if start != EXPECTED_START:
            failures.append(f"unqualified character started at {start!r}, expected {EXPECTED_START!r}")
        position, messages = _step(sock, 2)
        if "SPHERE_STEP_BLOCKED" not in messages:
            failures.append(f"unqualified @Step marker missing: {messages!r}")
        if position == PORTAL_DESTINATION:
            failures.append(f"unqualified character teleported to {position!r}")
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        failures.append(str(error))
    finally:
        if sock is not None:
            sock.close()

    sock = None
    try:
        sock = _enter(port, 1)
        start = _where(sock)
        if start != QUALIFIED_START:
            failures.append(f"qualified character started at {start!r}, expected {QUALIFIED_START!r}")
        _, messages = _step(sock, 2)
        if "SPHERE_STEP_ALLOWED" not in messages:
            failures.append(f"qualified @Step marker missing: {messages!r}")
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
    parser.add_argument("--port", type=int, default=2939)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    log_path = fixture / "server.log"
    failures: list[str] = []
    process = None
    returncode = None
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, args.startup_timeout)
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
        print("step portal probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1
    print("step portal probe passed: both @Step branches gated the portal")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
