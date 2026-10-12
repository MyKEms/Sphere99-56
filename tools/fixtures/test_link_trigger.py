#!/usr/bin/env python3
"""Verify dotted LINK and object-root TRIGGER dispatch."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "LinkTriggerProbe"
LOGIN_VALUE = "link-trigger-pw"
MARKER = "SPHERE_LINK_TRIGGER"
EXPECTED = (
    f"{MARKER}|login",
    f"{MARKER}|driver",
    f"{MARKER}|target-shorthand|03|1",
    f"{MARKER}|after-shorthand|1",
    f"{MARKER}|target-explicit|03|1",
    f"{MARKER}|after-explicit|1",
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
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
    parser.add_argument("--port", type=int, default=3178)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import decode_game_response, find_start_packet, game_connect, make_char_play, recv_until_game_start

    failures: list[str] = []
    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("linked-trigger probe did not reach the character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("linked-trigger probe did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 20.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if messages.count(f"{MARKER}|after_object") >= 1:
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except (ConnectionResetError, OSError):
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

    observed = [message for message in messages if message.startswith(f"{MARKER}|")]
    if observed != list(EXPECTED):
        failures.append(f"trigger marker sequence was {observed!r}; expected {list(EXPECTED)!r}")
    report = fixture / "logs" / "unknown-keywords.json"
    if report.is_file():
        try:
            entries = json.loads(report.read_text(encoding="utf-8"))
        except (OSError, ValueError) as error:
            failures.append(f"unknown-keyword report could not be read: {error}")
        else:
            if isinstance(entries, dict):
                entries = entries.get("entries", entries.get("records", []))
            if isinstance(entries, list):
                link_entries = [
                    entry
                    for entry in entries
                    if isinstance(entry, dict)
                    and "LINK.*" in str(entry.get("keyword", entry.get("key", "")))
                ]
                if link_entries:
                    failures.append(f"LINK.* remained in unknown-keyword report: {link_entries!r}")
    if failures:
        print("linked-trigger probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("observed messages:", messages, file=sys.stderr)
        return 1
    print("linked-trigger probe passed: LINK and object-root TRIGGER each dispatched the target")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
