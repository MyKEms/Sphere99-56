#!/usr/bin/env python3
"""Check numeric HVAL item creation and tag-derived EVENTS forms."""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from modes.newitem_events import ACCOUNT, MARKER, PASSWORD
from run_suite import shutdown_failures


END_MARKER = MARKER + "_END"
ROW_RE = re.compile(r"^" + re.escape(MARKER) + r" ([A-Z_]+)\|\[(.*)\]$")
EXPECTED = {
    "DIRECT": "synthetic newitem events",
    "NEWITEM_ZERO": "ready",
    "EVENTS_EMPTY": "ready",
    "HVAL_ARG": "synthetic newitem events",
    "HVAL_LITERAL": "synthetic newitem events",
    "HVAL_FULL_RESOURCE": "synthetic newitem events",
    "HVAL_ARGS": "synthetic newitem events",
    "HVAL_SRC_ARGS": "synthetic newitem events",
    "HVAL_ARGV": "synthetic newitem events",
    "HVAL_SAFE_ARGS": "synthetic newitem events",
    "EVENTS_PLAIN": "1",
    "EVENTS_PLUS": "1",
    "EVENTS_MINUS": "0",
    "EVENTS_QUOTED": "1",
    "EVENTS_INDEXED_TAG": "1",
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
            raise RuntimeError("NEWITEM/EVENTS probe world load did not finish")
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("NEWITEM/EVENTS probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("NEWITEM/EVENTS probe character did not enter the world")
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    rows: dict[str, str] = {}
    for message in messages:
        match = ROW_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2)
    for key, expected in EXPECTED.items():
        if rows.get(key) != expected:
            failures.append(f"{key}: got {rows.get(key)!r}; expected {expected!r}")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")
    command_errors = [
        line
        for line in log_contents.splitlines()
        if "Command 'newitem(" in line or "Unknown 'Events' Resource" in line
    ]
    if command_errors:
        failures.append(f"unexpected NEWITEM/EVENTS command errors: {command_errors!r}")
    is_item_errors = [
        line
        for line in log_contents.splitlines()
        if "Parse 'isItem)'" in line or "Eval 'isItem)'" in line
    ]
    if is_item_errors:
        failures.append(f"unexpected isItem evaluation errors: {is_item_errors!r}")

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        failures.append(f"unknown-keyword report unavailable: {error}")
    else:
        unexpected = [
            entry
            for entry in report.get("entries", [])
            if str(entry.get("keyword", "")).upper() in {"NEWITEM", "EVENTS"}
            and str(entry.get("kind", "")).lower() == "rejected"
        ]
        if unexpected:
            failures.append(f"target commands still reported rejected: {unexpected!r}")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print(
            f"NEWITEM/EVENTS probe failed: {max(0, len(EXPECTED) + 1 - len(failures))}/{len(EXPECTED) + 1} checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"NEWITEM/EVENTS probe passed: {len(EXPECTED) + 1}/{len(EXPECTED) + 1} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
