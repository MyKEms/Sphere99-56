#!/usr/bin/env python3
"""Check a resistance helper called from an item timer without a source."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

from modes.src_resists import ACCOUNT, BRANCH_MARKER, END_MARKER, MARKER, PASSWORD
from run_suite import shutdown_failures


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
    parser.add_argument("--port", type=int, default=43211)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
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
            raise RuntimeError("source-resists probe world load did not finish")
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("source-resists account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            if not recv_until_game_start(sock, timeout=30.0):
                raise RuntimeError("source-resists character did not enter the world")
            data = bytearray()
            deadline = time.monotonic() + 30.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                data.extend(chunk)
            messages[:] = system_messages(bytes(data))
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=args.binary.resolve(),
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
    rows = [message for message in messages if message.startswith(MARKER)]
    expected = {
        MARKER + "|[00]",
        MARKER + "_ISSUMMON|[00]",
        MARKER + "_TAG_TEXT|[<SRC.TAG(name)>]",
        MARKER + "_TAG_LENGTH|[<STRLEN(<SRC.TAG(name)>)>]",
        MARKER + "_NAME|[Sphere99 synthetic fixture]",
        BRANCH_MARKER + "|[<src.tag(x)>]",
    }
    missing = sorted(expected.difference(rows))
    if missing:
        failures.append(f"source-resists values differ from stock; missing={missing!r}; messages={messages!r}")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        failures.append(f"unknown-keyword report unavailable: {error}")
    else:
        rejected = [
            entry for entry in report.get("entries", [])
            if str(entry.get("keyword", "")).upper() in {"SRC.ISSUMMON"}
        ]
        if rejected:
            failures.append(f"missing-source SRC reads were unresolved: {rejected!r}")
        strlen = [
            entry for entry in report.get("entries", [])
            if str(entry.get("keyword", "")).upper() == "STRLEN"
        ]
        if strlen:
            failures.append(f"preserved null-source STRLEN was reported unknown: {strlen!r}")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print(f"SRC-resists probe failed: {max(0, 8 - len(failures))}/8 checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("SRC-resists probe passed: 8/8 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
