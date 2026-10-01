#!/usr/bin/env python3
"""Verify that a refused plane-5 step is cancelled without a server crash."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
from pathlib import Path

from run_suite import shutdown_failures, stop_server, tail, wait_for_port


ACCOUNT = "MovementProbe"
PASSWORD = "movement_pw"
SANITIZER_MARKERS = (
    "AddressSanitizer",
    "UndefinedBehaviorSanitizer",
    "runtime error:",
)


def _walk_packet(direction: int, sequence: int) -> bytes:
    return struct.pack(">BBBI", 0x02, direction, sequence & 0xFF, 0)


def _decoded_packets(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _recv_until_walk_reply(sock: socket.socket, timeout: float = 3.0) -> list:
    import time

    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            packets = _decoded_packets(bytes(data))
            if any(packet.command in (0x21, 0x22) for packet in packets):
                return packets
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                return _decoded_packets(bytes(data))
            data.extend(chunk)
    finally:
        sock.setblocking(True)
    return _decoded_packets(bytes(data))


def _enter(port: int) -> socket.socket:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect("127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("plane walk fixture did not reach the character list")
    sock.sendall(make_char_play(0))
    initial = decode_game_response(recv_until_game_start(sock, timeout=10.0))
    if find_start_packet(initial) is None:
        sock.close()
        raise RuntimeError("plane walk fixture character did not enter the world")
    version = b"3.0.9.0.1"
    sock.sendall(bytes([0xBD]) + (3 + len(version)).to_bytes(2, "big") + version)
    return sock


def run_probe(port: int) -> list[str]:
    failures: list[str] = []
    sock = None
    try:
        sock = _enter(port)
        # The first packet establishes the facing direction.  The second is
        # the valid plane-5 step whose placement is refused by master.
        sock.sendall(_walk_packet(0, 1))
        first = _recv_until_walk_reply(sock)
        if not any(packet.command == 0x22 for packet in first):
            failures.append("initial walk direction did not receive an acknowledgement")
        sock.sendall(_walk_packet(0, 2))
        second = _recv_until_walk_reply(sock)
        cancels = [packet for packet in second if packet.command == 0x21]
        if not cancels:
            failures.append("refused plane-5 step did not receive a walk cancel")
        else:
            packet = cancels[-1]
            if len(packet.data) < 8:
                failures.append("walk cancel did not include the current position")
            else:
                x, y = struct.unpack_from(">HH", packet.data, 2)
                if (x, y) != (128, 128):
                    failures.append(
                        f"walk cancel moved the character to ({x},{y}), expected (128,128)"
                    )
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
    parser.add_argument("--port", type=int, default=2937)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    log_path = fixture / "server.log"
    process: subprocess.Popen[bytes] | None = None
    failures: list[str] = []
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
    refused_marker = "Event_Walk: MoveToChar refused destination"
    if log_contents.count(refused_marker) != 1:
        failures.append(
            f"server log has {log_contents.count(refused_marker)} refused-step markers; expected 1"
        )
    failures.extend(
        f"server log contains sanitizer output: {line}"
        for line in log_contents.splitlines()
        if any(marker in line for marker in SANITIZER_MARKERS)
    )
    if failures:
        print("plane-walk-crash probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1
    print("plane-walk-crash probe passed: refused plane-5 step was cancelled")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
