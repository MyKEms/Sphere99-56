#!/usr/bin/env python3
"""Verify the bare EVENTS(...) add/remove method on a live character."""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import (
    EVENTS_METHOD_ACCOUNT,
    EVENTS_METHOD_EVENT,
    EVENTS_METHOD_MARKER,
    EVENTS_METHOD_PASSWORD,
)
from run_suite import shutdown_failures


SANITIZER_RE = re.compile(
    r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|"
    r"ThreadSanitizer|MemorySanitizer|runtime error:",
    re.IGNORECASE,
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(
        decode_game_response(data), allow_truncated=True
    ):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def wait_for_world_load(log_path: Path, timeout: float) -> None:
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
    parser.add_argument("--port", type=int, default=2862)
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
        wait_for_world_load(fixture / "server.log", args.startup_timeout)
        sock, _ = game_connect(
            args.host,
            args.port,
            EVENTS_METHOD_ACCOUNT,
            EVENTS_METHOD_PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("EVENTS method probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("EVENTS method probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if f"{EVENTS_METHOD_MARKER}_END" in messages:
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
    if SANITIZER_RE.search(log_contents):
        failures.append("server log contains sanitizer output")

    add_markers = [
        message
        for message in messages
        if message.startswith(f"{EVENTS_METHOD_MARKER}_ADD ")
    ]
    remove_markers = [
        message
        for message in messages
        if message.startswith(f"{EVENTS_METHOD_MARKER}_REMOVE ")
    ]
    if len(add_markers) != 1:
        failures.append(f"EVENTS add marker count: {len(add_markers)}")
    elif EVENTS_METHOD_EVENT not in add_markers[0]:
        failures.append(f"EVENTS add list did not contain {EVENTS_METHOD_EVENT}")
    if len(remove_markers) != 1:
        failures.append(f"EVENTS remove marker count: {len(remove_markers)}")
    elif EVENTS_METHOD_EVENT in remove_markers[0]:
        failures.append(f"EVENTS remove list still contained {EVENTS_METHOD_EVENT}")
    property_remove_markers = [
        message
        for message in messages
        if message.startswith(f"{EVENTS_METHOD_MARKER}_PROPERTY_REMOVE ")
    ]
    if len(property_remove_markers) != 1:
        failures.append(
            f"EVENTS property-remove marker count: {len(property_remove_markers)}"
        )
    elif EVENTS_METHOD_EVENT in property_remove_markers[0]:
        failures.append(
            f"EVENTS property-remove list still contained {EVENTS_METHOD_EVENT}"
        )
    rejected_events = [
        line for line in log_contents.splitlines() if "rejected EVENTS" in line
    ]
    if rejected_events:
        failures.append(f"EVENTS missing-remove was rejected: {rejected_events[0]}")
    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        failures.append(f"unknown-keyword report unavailable: {error}")
    else:
        rejected = [
            entry
            for entry in report.get("entries", [])
            if isinstance(entry, dict)
            and entry.get("kind") == "rejected"
            and entry.get("keyword") == "EVENTS"
        ]
        if rejected:
            failures.append(f"unknown-keyword report rejected EVENTS: {rejected[0]}")
    end_marker = f"{EVENTS_METHOD_MARKER}_END"
    if messages.count(end_marker) != 1:
        failures.append(
            f"end marker count: {messages.count(end_marker)}"
        )

    total = 6
    if failures:
        print(
            f"EVENTS method probe failed: {max(0, total - len(failures))}/{total} checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"EVENTS method probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
