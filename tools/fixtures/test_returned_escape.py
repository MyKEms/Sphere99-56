#!/usr/bin/env python3
"""Check that script-function return text is expanded exactly once at use."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures
from modes.returned_escape import ACCOUNT, CHAR_NAME, DIALOG_NAME, EXPECTED_TEXTS, ITEM_UID, MARKER, PASSWORD


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return [
        packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        for packet in split_packet_stream(decode_game_response(data), allow_truncated=True)
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=45228)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_gumps import parse_gump_dialog
    from uo_packets import split_packet_stream
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_relogin,
        make_char_play,
        recv_until_game_start,
    )

    failures: list[str] = []
    messages: list[str] = []

    def exercise() -> None:
        sock, _, initial = game_relogin(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("returned-escape account did not reach the character list")
        del initial
        try:
            sock.sendall(make_char_play(0))
            raw = bytearray(recv_until_game_start(sock, timeout=30.0))
            if find_start_packet(decode_game_response(bytes(raw))) is None:
                raise RuntimeError("returned-escape character did not enter the world")

            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline and f"{MARKER}|nested={CHAR_NAME}" not in messages:
                messages[:] = system_messages(bytes(raw))
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                raw.extend(chunk)
            messages[:] = system_messages(bytes(raw))

            sock.sendall(bytes([0x06]) + ITEM_UID.to_bytes(4, "big"))
            deadline = time.monotonic() + 10.0
            dialog = None
            seen = 0
            while time.monotonic() < deadline and dialog is None:
                packets = split_packet_stream(decode_game_response(bytes(raw)), allow_truncated=True)
                for packet in packets[seen:]:
                    seen += 1
                    if packet.command in (0xB0, 0xDD):
                        dialog = parse_gump_dialog(packet.data)
                        break
                if dialog is not None:
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                raw.extend(chunk)
            if dialog is None:
                failures.append("returned-escape dialog did not arrive")
            elif tuple(dialog.texts) != EXPECTED_TEXTS:
                failures.append(f"dialog texts {tuple(dialog.texts)!r}; expected {EXPECTED_TEXTS!r}")
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
    expected_messages = {
        f"{MARKER}|direct={CHAR_NAME}",
        f"{MARKER}|nested={CHAR_NAME}",
        f"{MARKER}|left=abc",
        f"{MARKER}|trap_can=1",
    }
    missing = expected_messages.difference(messages)
    failures.extend(f"missing system marker {marker!r}" for marker in sorted(missing))
    literal = [message for message in messages if "<SRC.NAME>" in message or "<f_returned_escape" in message]
    if literal:
        failures.append(f"returned escape remained literal: {literal!r}")
    if failures:
        print("returned-escape probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("returned-escape probe passed: system markers and dialog text expanded")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
