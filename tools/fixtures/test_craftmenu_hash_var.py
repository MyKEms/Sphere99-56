#!/usr/bin/env python3
"""Verify a craft-menu source root stored as a persisted ``#<serial>`` VAR."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.craftmenu_hash_var import ACCOUNT, END_MARKER, LOGIN_VALUE, MARKER
from run_suite import shutdown_failures


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return [
        packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        for packet in split_packet_stream(decode_game_response(data), allow_truncated=True)
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def exercise(args: argparse.Namespace, failures: list[str]) -> None:
    from uo_packets import split_packet_stream
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        args.host,
        args.port,
        ACCOUNT,
        LOGIN_VALUE,
        game_port=args.port + 1000,
    )
    if sock is None:
        failures.append("craft hash VAR account did not reach its character list")
        return
    try:
        sock.sendall(make_char_play(0))
        response = recv_until_game_start(sock, timeout=30.0)
        if not response or find_start_packet(decode_game_response(response)) is None:
            failures.append("craft hash VAR character did not enter the world")
            return

        raw = bytearray(response)
        packet_index = 0
        messages: list[str] = []

        def next_packet(timeout: float):
            nonlocal packet_index
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                packets = split_packet_stream(
                    decode_game_response(bytes(raw)), allow_truncated=True
                )
                if packet_index < len(packets):
                    packet = packets[packet_index]
                    packet_index += 1
                    return packet
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    return None
                if not chunk:
                    return None
                raw.extend(chunk)
            return None

        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline and END_MARKER not in messages:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            messages[:] = system_messages(bytes(raw))
        required_markers = (
            MARKER + "|root-ok",
            MARKER + "|item-ok",
            MARKER + "|opened",
            END_MARKER,
        )
        if not any(
            message.startswith(MARKER + "|stored|[#")
            for message in messages
        ):
            failures.append(f"persisted hash VAR marker was missing: {messages!r}")
        if any(marker not in messages for marker in required_markers):
            failures.append(f"craft hash VAR markers were incomplete: {messages!r}")
            return
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3172)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server

    failures: list[str] = []
    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=fixture / "server.log",
        action=lambda: exercise(args, failures),
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print("craft hash VAR probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("craft hash VAR probe passed: persisted source root resolved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
