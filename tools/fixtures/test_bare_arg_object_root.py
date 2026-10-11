#!/usr/bin/env python3
"""Verify property and method dispatch through a bare ARG object root."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.bare_arg_object_root import ACCOUNT, END_MARKER, MARKER, PASSWORD
from run_suite import shutdown_failures

ROW_RE = re.compile(re.escape(MARKER) + r"\|([a-z0-9_]+)\|\[(.*)\]")


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
    parser.add_argument("--port", type=int, default=3174)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import decode_game_response, find_start_packet, game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        # Let the seeded spawn item create its NPC before the player login
        # schedules the sector-wide probe callback.
        time.sleep(2.5)
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            failures.append("bare ARG object-root account did not reach its character list")
            return
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                failures.append("bare ARG object-root character did not enter the world")
                return
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

    rows: dict[str, str] = {}
    for message in messages:
        match = ROW_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2)
    if "serial" not in rows or not rows["serial"].startswith("04"):
        failures.append(f"object serial row {rows.get('serial')!r} is missing")
    expected_rows = {
        "memory": "1",
        "before": "128,128,0",
        "after": "128,128,0",
        "more2": "011",
        "tag": "17",
        "guard": "passed",
        "bareif": "passed",
    }
    for key, expected in expected_rows.items():
        if rows.get(key) != expected:
            failures.append(f"{key}: got {rows.get(key)!r}; expected {expected!r}")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")
    if re.search(r"(?:mySpawn|MYSPAWN)\.(?:serial|p|more2|tag)", log_contents):
        failures.append("bare ARG root was rejected in the server log")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("bare ARG object-root probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    check_count = 1 + len(expected_rows) + 1  # serial, value rows, and end marker
    print(f"bare ARG object-root probe passed: {check_count}/{check_count} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
