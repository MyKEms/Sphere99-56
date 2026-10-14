#!/usr/bin/env python3
"""Check that SAFE preserves a command's side effect and suppresses errors."""

from __future__ import annotations

import argparse
import json
import socket
import sys
import time
from pathlib import Path

from modes.safe_command import (
    ACCOUNT,
    MACRO_MARKER,
    MEMORY_MARKER,
    MARKER,
    PASSWORD,
)
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
            raise RuntimeError("SAFE command probe world load did not finish")
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("SAFE command probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("SAFE command probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if messages.count(MARKER + " END") == 1:
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

    callback = MARKER + " CALLBACK"
    message = MARKER + " MESSAGE"
    end = MARKER + " END"
    if messages.count(callback) != 1:
        failures.append(f"command callback count {messages.count(callback)} != 1")
    if messages.count(MACRO_MARKER) != 1:
        failures.append(f"macro command callback count {messages.count(MACRO_MARKER)} != 1")
    if messages.count(MEMORY_MARKER) != 1:
        failures.append(f"memory command callback count {messages.count(MEMORY_MARKER)} != 1")
    if messages.count(message) != 1:
        failures.append(f"message count {messages.count(message)} != 1")
    if messages.count(end) != 1:
        failures.append(f"end marker count {messages.count(end)} != 1")
    try:
        callback_index = messages.index(callback)
        message_index = messages.index(message)
        macro_index = messages.index(MACRO_MARKER)
        memory_index = messages.index(MEMORY_MARKER)
        if (
            callback_index >= message_index
            or macro_index >= message_index
            or memory_index >= message_index
        ):
            failures.append("command callback did not precede the message")
    except ValueError:
        pass

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (OSError, ValueError) as error:
        failures.append(f"unknown-keyword report is unreadable: {error}")
    else:
        rejected = [
            entry
            for entry in report.get("entries", [])
            if entry.get("kind") == "rejected" and entry.get("keyword") == "EQUIP"
        ]
        if rejected:
            failures.append(f"SAFE EQUIP rejection leaked into report: {rejected!r}")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("SAFE command probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("SAFE command probe passed: 7/7 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
