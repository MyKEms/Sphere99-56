#!/usr/bin/env python3
"""Check that a handled item typedef trigger stops default item use."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.typedef_trigger_return import (
    ACCOUNT,
    BASE_MARKER,
    HANDLED_MARKER,
    MARKER,
    PASSWORD,
    READY_MARKER,
)
from run_suite import shutdown_failures


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
    parser.add_argument("--port", type=int, default=43217)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    item_uid: int | None = None

    def exercise() -> None:
        nonlocal item_uid
        deadline = time.monotonic() + args.startup_timeout
        while time.monotonic() < deadline:
            try:
                if "world load diagnostics:" in (fixture / "server.log").read_text(
                    encoding="utf-8", errors="replace"
                ):
                    break
            except OSError:
                pass
            time.sleep(0.1)
        else:
            raise RuntimeError("typedef trigger probe world load did not finish")
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("typedef trigger probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("typedef trigger probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                ready = next((message for message in messages if message.startswith(READY_MARKER + "|")), None)
                if ready is not None:
                    # Sphere prints serials as bare hexadecimal, including a
                    # leading zero for a resource UID (e.g. 040000001).
                    item_uid = int(ready.split("|")[1], 16)
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                data.extend(chunk)
            if item_uid is None:
                raise RuntimeError("typedef trigger probe did not publish an item UID")

            sock.sendall(bytes((0x06,)) + item_uid.to_bytes(4, "big"))
            deadline = time.monotonic() + 8.0
            handled_at: float | None = None
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if messages.count(HANDLED_MARKER) >= 1:
                    handled_at = handled_at or time.monotonic()
                    # Keep collecting briefly: the unfixed path emits the
                    # default callback/diagnostic after the typedef handler.
                    if messages.count(BASE_MARKER) or any(
                        "You can't think of a way to use that item" in message
                        for message in messages
                    ):
                        break
                    if time.monotonic() - handled_at >= 1.0:
                        break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
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
    if item_uid is None:
        failures.append("handled typedef trigger did not publish a target item")
    if messages.count(HANDLED_MARKER) != 1:
        failures.append(f"handled callback count {messages.count(HANDLED_MARKER)} != 1")
    if messages.count(BASE_MARKER) != 0:
        failures.append(f"default item callback count {messages.count(BASE_MARKER)} != 0")
    if any("You can't think of a way to use that item" in message for message in messages):
        failures.append("default item-use diagnostic leaked after handled typedef trigger")
    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("typedef trigger probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("typedef trigger probe passed: 3/3 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
