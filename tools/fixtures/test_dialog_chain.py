#!/usr/bin/env python3
"""Check that a dotted TAG result can dispatch a nested dialog."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures
from modes.dialog_chain import ACCOUNT, LOGIN_VALUE, MARKER


def system_messages(data: bytes) -> list[str]:
    from uo_packets import gump_dialog_controls, split_packet_stream
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
    parser.add_argument("--port", type=int, default=45218)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_gumps import make_gump_reply, parse_gump_dialog
    from uo_packets import gump_dialog_controls, split_packet_stream
    from uo_test_client import decode_game_response, find_start_packet, game_connect, make_char_play, recv_until_game_start

    failures: list[str] = []
    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(args.host, args.port, ACCOUNT, LOGIN_VALUE, game_port=args.port + 1000)
        if sock is None:
            raise RuntimeError("dialog-chain account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            raw = bytearray(recv_until_game_start(sock, timeout=30.0))
            if find_start_packet(decode_game_response(bytes(raw))) is None:
                raise RuntimeError("dialog-chain character did not enter the world")
            seen = 0
            deadline = time.monotonic() + 15.0
            gump = None
            sock.settimeout(0.2)
            while time.monotonic() < deadline and gump is None:
                packets = split_packet_stream(decode_game_response(bytes(raw)), allow_truncated=True)
                for packet in packets[seen:]:
                    seen += 1
                    messages[:] = system_messages(bytes(raw))
                    if packet.command in (0xB0, 0xDD):
                        try:
                            candidate = parse_gump_dialog(packet.data)
                        except ValueError as error:
                            failures.append(f"nested dialog packet was malformed: {error}")
                            return
                        if candidate.find_button(button_id=7) is not None:
                            controls = [
                                " ".join(control.split())
                                for control in gump_dialog_controls(packet.data)
                            ]
                            if sum(control.startswith("Text ") for control in controls) < 2:
                                failures.append(
                                    "plain text controls containing dots were not preserved: "
                                    f"{controls!r}"
                                )
                                return
                            if not any(control.startswith("HTMLGump ") for control in controls):
                                failures.append(
                                    "decimal/quoted dotted layout control was not preserved: "
                                    f"{controls!r}"
                                )
                                return
                            gump = candidate
                            break
                if gump is not None:
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                raw.extend(chunk)
            messages[:] = system_messages(bytes(raw))
            if gump is None:
                failures.append(f"dotted TAG dialog did not open its nested gump: {messages!r}")
                return
            sock.sendall(make_gump_reply(gump.serial, gump.context, 7, switches=(0,)))
            deadline = time.monotonic() + 10.0
            while time.monotonic() < deadline and f"{MARKER}|button|7" not in messages:
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                raw.extend(chunk)
                messages[:] = system_messages(bytes(raw))
            if f"{MARKER}|button|7" not in messages:
                failures.append(f"nested dialog button callback was absent: {messages!r}")
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
    if failures:
        print("dialog-chain probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("dialog-chain probe passed: nested TAG dialog and button callback")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
