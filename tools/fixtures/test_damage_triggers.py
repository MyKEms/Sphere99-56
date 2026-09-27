#!/usr/bin/env python3
"""Verify item @Damage and source-character @ItemDamage dispatch."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from make_fixture import (
    DAMAGE_TRIGGER_ACCOUNT,
    DAMAGE_TRIGGER_MARKER,
    DAMAGE_TRIGGER_PASSWORD,
)
from run_suite import shutdown_failures


ITEM_MARKER = DAMAGE_TRIGGER_MARKER + " ITEM"
CHAR_MARKER = DAMAGE_TRIGGER_MARKER + " CHAR"
END_MARKER = DAMAGE_TRIGGER_MARKER + "_END"


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def wait_for_world_load(log_path: Path, timeout: float = 60.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            log = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            log = ""
        if "world load:" in log:
            return
        time.sleep(0.1)
    raise RuntimeError("server did not finish world load before the bounded timeout")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2860)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        wait_for_world_load(fixture / "server.log")
        sock, _ = game_connect(
            args.host,
            args.port,
            DAMAGE_TRIGGER_ACCOUNT,
            DAMAGE_TRIGGER_PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError(
                "damage-trigger probe account did not reach its character list"
            )
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("damage-trigger probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
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

    if messages.count(ITEM_MARKER) != 1:
        failures.append(f"item @Damage marker count: {messages.count(ITEM_MARKER)}")
    if messages.count(CHAR_MARKER) != 1:
        failures.append(f"character @ItemDamage marker count: {messages.count(CHAR_MARKER)}")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count: {messages.count(END_MARKER)}")

    total = 3
    if failures:
        print(
            f"damage-trigger probe failed: "
            f"{max(0, total - len(failures))}/{total} checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"damage-trigger probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
