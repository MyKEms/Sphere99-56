#!/usr/bin/env python3
"""Check that ARG1S method arguments lose only their outer quote pair."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.arg1s_quotes import ACCOUNT, LOGIN_TOKEN, MARKER


EXPECTED = {
    f"{MARKER}|verb-quoted|5",
    f"{MARKER}|verb-plain",
    f"{MARKER}|call-quoted|7",
    f"{MARKER}|call-plain",
    f"{MARKER}|dotted-quoted|9",
    f"{MARKER}|dotted-plain",
}


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=43221)
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
            args.host, args.port, ACCOUNT, LOGIN_TOKEN, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("ARG1S probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("ARG1S probe character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline and not EXPECTED.issubset(messages):
                messages[:] = system_messages(bytes(buffer))
                if EXPECTED.issubset(messages):
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
    from run_suite import shutdown_failures

    failures.extend(shutdown_failures(returncode, log_contents))
    missing = sorted(EXPECTED.difference(messages))
    if missing:
        failures.append(f"missing expected messages: {missing}")
    if any(message.startswith(f'{MARKER}|') and '"' in message for message in messages):
        failures.append("ARG1S output retained an outer quote")

    if failures:
        print("ARG1S quote probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print(f"ARG1S quote probe passed: {len(EXPECTED)} forms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
