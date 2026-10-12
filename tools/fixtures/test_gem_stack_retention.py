#!/usr/bin/env python3
"""Verify a script-created pile preserves the saved pile's UID."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures

from modes.gem_stack_retention import ACCOUNT, EXISTING_SERIAL, MARKER, PASSWORD


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def _parse_marker(messages: list[str]) -> tuple[int, int, int] | None:
    prefix = f"{MARKER} ["
    for message in messages:
        if not message.startswith(prefix) or not message.endswith("]"):
            continue
        values = message[len(prefix) : -1].split("|")
        if len(values) != 3:
            return None
        try:
            parsed = []
            for value in values[:2]:
                token = value.strip().lstrip("#")
                try:
                    parsed.append(int(token, 0))
                except ValueError:
                    parsed.append(int(token, 16))
            parsed.append(int(values[2].strip(), 0))
            return tuple(parsed)  # type: ignore[return-value]
        except ValueError:
            return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3210)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("gem-stack probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("gem-stack probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if any(message.startswith(MARKER) for message in messages):
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            messages[:] = system_messages(bytes(data))
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    observed = _parse_marker(messages)
    expected_uid = 0x40000000 | EXISTING_SERIAL
    if observed != (expected_uid, expected_uid, 6):
        failures.append(
            "saved pile was not retained while adding the script pile: "
            f"observed={observed!r} expected={(expected_uid, expected_uid, 6)!r}"
        )
    if failures:
        print("gem-stack retention probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("gem-stack retention probe passed: saved pile UID and amount survived script addition")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
