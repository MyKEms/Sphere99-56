#!/usr/bin/env python3
"""Verify object-valued CONT assignments inside a dynamic contents callback."""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures

from modes.dynamic_contents import (
    ACCOUNT,
    CHILD_UID,
    DESTINATION_UID,
    MARKER,
    PASSWORD,
    SOURCE_UID,
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def _parse_row(messages: list[str], label: str) -> tuple[int, ...] | None:
    prefix = f"{MARKER}_{label} "
    for message in messages:
        if not message.startswith(prefix):
            continue
        values = message[len(prefix) :].split("|")
        try:
            return tuple(int(value.strip().lstrip("#"), 16) for value in values)
        except ValueError:
            return None
    return None


def _rejected_cont_entries(fixture: Path) -> list[dict[str, object]]:
    report = fixture / "logs" / "unknown-keywords.json"
    try:
        payload = json.loads(report.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return []
    return [
        entry
        for entry in payload.get("entries", [])
        if isinstance(entry, dict)
        and entry.get("kind") == "rejected"
        and str(entry.get("keyword", "")).upper() == "CONT"
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3150)
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
            raise RuntimeError("dynamic contents probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("dynamic contents probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if f"{MARKER}_END" in messages:
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
    if messages.count(f"{MARKER}_END") != 1:
        failures.append(f"end marker count {messages.count(f'{MARKER}_END')} != 1")

    before = _parse_row(messages, "BEFORE")
    if before != (SOURCE_UID, 1, DESTINATION_UID):
        failures.append(
            f"before row {before!r} != {(SOURCE_UID, 1, DESTINATION_UID)!r}"
        )
    after = _parse_row(messages, "AFTER")
    if after != (DESTINATION_UID, DESTINATION_UID, DESTINATION_UID):
        failures.append(
            f"after row {after!r} != "
            f"{(DESTINATION_UID, DESTINATION_UID, DESTINATION_UID)!r}"
        )
    rejected = _rejected_cont_entries(fixture)
    if rejected:
        failures.append(f"dynamic CONT assignment was rejected: {rejected!r}")
    if failures:
        print("dynamic contents probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print(
        "dynamic contents probe passed: child moved from "
        f"0x{SOURCE_UID:x} to 0x{DESTINATION_UID:x} without rejected CONT"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
