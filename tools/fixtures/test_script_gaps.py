#!/usr/bin/env python3
"""Check FINDCONT enumeration and the FLAG_IMMOBILE character flag."""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "ScriptGapsProbe"
PW = "script-gaps-pw"
MARKER = "SPHERE_SCRIPT_GAPS"
ROW_RE = re.compile(re.escape(MARKER) + r" ([a-z0-9_]+)(?:\|\[(.*)\]|)$")
EXPECTED = {
    "flag_initial": "0",
    "flag_property": "1",
    "flag_method": "0",
    "find0": "synthetic gap child one",
    "find1": "synthetic gap child two",
    "find2": "",
    "safe0": "0",
    "trigger": "1",
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
    parser.add_argument("--port", type=int, default=2880)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PW,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("script-gaps probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("script-gaps probe character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
                if f"{MARKER} find2|[]" in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                buffer.extend(chunk)
            messages[:] = system_messages(bytes(buffer))
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
            rows[match.group(1)] = match.group(2) or ""
    for key, expected in EXPECTED.items():
        if rows.get(key) != expected:
            failures.append(f"{key}: expected {expected!r}, got {rows.get(key)!r}")
    if f"{MARKER} setup" not in messages:
        failures.append("setup marker missing")

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        failures.append(f"unknown-keyword report unavailable: {error}")
    else:
        unexpected = [
            entry
            for entry in report.get("entries", [])
            if str(entry.get("keyword", "")).upper() in {"FINDCONT", "FLAG_IMMOBILE"}
        ]
        if unexpected:
            failures.append(f"target keywords still reported unknown: {unexpected!r}")

    total = len(EXPECTED) + 2
    if failures:
        print(f"script-gaps probe failed: {max(0, total - len(failures))}/{total} checks passed", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"script-gaps probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
