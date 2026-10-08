#!/usr/bin/env python3
"""Check that drinking a potion dispatches its source-item spell callback."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.potion_effect import ACCOUNT, MARKER, PASSWORD, READY_MARKER
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


def potion_uid(messages: list[str]) -> int | None:
    prefix = READY_MARKER + " "
    for message in messages:
        if message.startswith(prefix):
            raw = message[len(prefix) :].strip().lstrip("#")
            try:
                return int(raw, 16)
            except ValueError:
                try:
                    return int(raw, 10)
                except ValueError:
                    return None
    return None


def make_dclick(uid: int) -> bytes:
    """Build the classic five-byte double-click packet."""

    return bytes((0x06,)) + uid.to_bytes(4, "big")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3156)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    data = bytearray()
    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("potion-effect probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("potion-effect probe character did not enter the world")
            data.extend(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if potion_uid(messages) is not None:
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            uid = potion_uid(messages)
            if uid is None:
                return
            sock.sendall(make_dclick(uid))
            deadline = time.monotonic() + 8.0
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if MARKER in messages:
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
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    ready_count = sum(message.startswith(READY_MARKER + " ") for message in messages)
    effect_count = sum(message == MARKER for message in messages)
    if ready_count != 1:
        failures.append(f"ready marker count {ready_count} != 1")
    if effect_count != 1:
        failures.append(f"source-item potion-effect marker count {effect_count} != 1")

    if failures:
        print("potion-effect probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("potion-effect probe passed: source-item potion-effect callback ran once")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
