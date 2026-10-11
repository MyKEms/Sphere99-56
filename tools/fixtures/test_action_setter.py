#!/usr/bin/env python3
"""Check ACTION skill start and cancellation writes."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures

from modes.action_setter import ACCOUNT, LOGIN_VALUE, MARKER


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
    parser.add_argument("--port", type=int, default=3176)
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
            args.host, args.port, ACCOUNT, LOGIN_VALUE, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("ACTION setter probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("ACTION setter character did not enter the world")
            data = bytearray(response)
            sock.settimeout(0.2)
            deadline = time.monotonic() + 12.0
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

    # Skill definitions are not entered in the legacy resource-name hash, so
    # ACTION's existing getter expands an active skill and an idle character
    # as '?'.  The exact marker matters: it proves the getter ran after each
    # setter, while the rejection report proves that the writes did not fall
    # through to the unknown-command path.
    expected = {
        "valid": f"{MARKER} valid|[?]",
        "minus-one": f"{MARKER} minus-one|[?]",
        "empty": f"{MARKER} empty|[?]",
    }
    for label, wanted in expected.items():
        if wanted not in messages:
            failures.append(f"{label}: missing {wanted!r}")
    if f"{MARKER}_END" not in messages:
        failures.append("ACTION setter probe did not reach its end marker")

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (FileNotFoundError, OSError, json.JSONDecodeError) as error:
        failures.append(f"unknown-keyword report is unreadable: {error}")
    else:
        action_entries = [
            entry
            for entry in report.get("entries", [])
            if isinstance(entry, dict)
            and str(entry.get("kind", "")).lower() == "set"
            and str(entry.get("keyword", "")).upper() == "ACTION"
        ]
        if action_entries:
            failures.append(f"valid ACTION writes were rejected: {action_entries!r}")

    if failures:
        print("ACTION setter probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"observed messages: {messages!r}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print("ACTION setter probe passed: valid skill and both cancellation forms")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
