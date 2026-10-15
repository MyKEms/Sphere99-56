#!/usr/bin/env python3
"""Check stock-compatible resource-only CANMAKE and MAKEITEM behavior."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "ResourceOnlyMakeProbe"
PASSWORD = "res-pw"
MARKER = "SPHERE_RESOURCE_ONLY_MAKE"
EXPECTED = {"empty": "1", "rule": "1", "resource": "1", "missing": "0"}
ROW_RE = re.compile(re.escape(MARKER) + r"\|(\w+)=(.*)$")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return [
        packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        for packet in split_packet_stream(decode_game_response(data), allow_truncated=True)
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3212)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("resource-only probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("resource-only probe character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
                if f"{MARKER}|done" in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                buffer.extend(chunk)
            messages[:] = system_messages(bytes(buffer))
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=fixture / "server.log",
        action=exercise,
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    seen: dict[str, str] = {}
    for message in messages:
        match = ROW_RE.fullmatch(message)
        if match:
            seen[match.group(1)] = match.group(2)
    for label, expected in EXPECTED.items():
        got = seen.get(label)
        if got != expected:
            failures.append(f"{label}: expected {expected!r}, got {got!r}")
    if not any("You put the synthetic resource-only make definition" in message for message in messages):
        failures.append("direct MAKEITEM did not report the crafted item")
    if f"{MARKER}|done" not in messages:
        failures.append("resource-only probe did not finish")

    if failures:
        print("resource-only probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print(f"resource-only probe passed: {len(EXPECTED)}/{len(EXPECTED)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
