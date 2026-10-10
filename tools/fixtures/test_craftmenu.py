#!/usr/bin/env python3
"""Verify a script-defined ``craftmenu(skill)`` opens and dispatches a gump."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.craftmenu import ACCOUNT, END_MARKER, LOGIN_VALUE, MARKER
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
    from uo_gumps import make_gump_reply, parse_gump_dialog
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
        failures.append("craftmenu account did not reach its character list")
        return
    try:
        sock.sendall(make_char_play(0))
        response = recv_until_game_start(sock, timeout=30.0)
        if not response or find_start_packet(decode_game_response(response)) is None:
            failures.append("craftmenu character did not enter the world")
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

        gump = None
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline and gump is None:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            messages[:] = system_messages(bytes(raw))
            if packet.command in (0xB0, 0xDD):
                try:
                    gump = parse_gump_dialog(packet.data)
                except ValueError as error:
                    failures.append(f"craftmenu gump was malformed: {error}")
                    return
        if gump is None:
            failures.append(
                "script-defined craftmenu did not open a gump; "
                f"messages={messages!r}"
            )
            return
        required_markers = (
            MARKER + "|opened",
            MARKER + "|source-ok",
            END_MARKER,
        )
        if any(marker not in messages for marker in required_markers):
            failures.append(f"craftmenu open markers were incomplete: {messages!r}")
            return

        ref_prefixes = (
            MARKER + "|var-newitem=",
            MARKER + "|argv-newitem=",
            MARKER + "|argv-z=",
        )
        ref_values = {}
        for prefix in ref_prefixes:
            matches = [message[len(prefix):] for message in messages if message.startswith(prefix)]
            if len(matches) != 1 or not matches[0]:
                failures.append(f"craftmenu object-root marker was incomplete for {prefix!r}: {messages!r}")
                return
            ref_values[prefix] = matches[0]
        if ref_values[ref_prefixes[0]] == ref_values[ref_prefixes[1]]:
            failures.append(
                "VAR(...).NEWITEM and ARGV(0).NEWITEM did not create distinct items: "
                f"{ref_values!r}"
            )
            return
        if ref_values[ref_prefixes[2]] != "10":
            failures.append(
                "ARGV(0).Z assignment did not retain the referenced character: "
                f"expected 10, got {ref_values[ref_prefixes[2]]!r}; messages={messages!r}"
            )
            return

        sock.sendall(make_gump_reply(gump.serial, gump.context, 7, switches=(0,)))
        marker = MARKER + "|button|7"
        deadline = time.monotonic() + 10.0
        while marker not in messages and time.monotonic() < deadline:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            messages[:] = system_messages(bytes(raw))
        if marker not in messages:
            failures.append(f"craftmenu button callback did not produce {marker!r}: {messages!r}")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3164)
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
        print("craftmenu probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("craftmenu probe passed: menu opened and button callback dispatched")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
