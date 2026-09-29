#!/usr/bin/env python3
"""Verify that DIALOG call arguments reach the layout as positional ARGV."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from make_fixture import (
    DIALOG_ARGV_ACCOUNT,
    DIALOG_ARGV_FORWARD_BUTTON,
    DIALOG_ARGV_MARKER,
    DIALOG_ARGV_PASSWORD,
)
from run_suite import shutdown_failures


def system_message(data: bytes) -> Optional[str]:
    if len(data) < 45 or data[0] != 0x1C:
        return None
    return data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str]) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    # pylint: disable=import-outside-toplevel
    from uo_packets import (
        gump_dialog_controls,
        make_gump_reply,
        parse_gump_dialog,
        split_packet_stream,
    )
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        args.host,
        args.port,
        DIALOG_ARGV_ACCOUNT,
        DIALOG_ARGV_PASSWORD,
        game_port=args.port + 1000,
    )
    if sock is None:
        raise RuntimeError("dialog-argv account did not reach the character list")
    try:
        sock.sendall(
            make_char_create(
                name=DIALOG_ARGV_ACCOUNT,
                sex=0,
                start_loc=1,
                skill1=25,
                val1=40,
                skill2=26,
                val2=40,
                skill3=1,
                val3=20,
            )
        )
        raw = bytearray(recv_until_game_start(sock, timeout=30.0))
        if find_start_packet(decode_game_response(bytes(raw))) is None:
            raise RuntimeError("dialog-argv character did not enter the world")
        seen = 0

        def wait_for(match: Callable[[bytes], bool], timeout: float = 10.0) -> Optional[bytes]:
            nonlocal seen
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while True:
                packets = split_packet_stream(
                    decode_game_response(bytes(raw)), allow_truncated=True
                )
                for index in range(seen, len(packets)):
                    if match(packets[index].data):
                        seen = index + 1
                        return packets[index].data
                seen = len(packets)
                if time.monotonic() >= deadline:
                    return None
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    return None
                raw.extend(chunk)

        gump = wait_for(lambda data: parse_gump_dialog(data) is not None)
        if gump is None:
            failures.append("dialog was not opened")
            return
        controls = [" ".join(control.split()) for control in gump_dialog_controls(gump)]
        expected = f"button 20 20 2151 2152 1 0 {DIALOG_ARGV_FORWARD_BUTTON}"
        if controls != [expected]:
            failures.append(f"layout controls {controls!r}; expected {[expected]!r}")
            return
        passed.append("dialog ARGV selected the expected control")

        serial, context = parse_gump_dialog(gump)
        sock.sendall(make_gump_reply(serial, context, DIALOG_ARGV_FORWARD_BUTTON, (), ()))
        marker = DIALOG_ARGV_MARKER + " "
        report_packet = wait_for(
            lambda data: (system_message(data) or "").startswith(marker)
        )
        report = None if report_packet is None else system_message(report_packet)[len(marker) :]
        if report != "forward":
            failures.append(f"button handler reported {report!r}; expected 'forward'")
        else:
            passed.append("dialog ARGV button handler")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2864)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    args.fixture = args.fixture.resolve()
    args.binary = args.binary.resolve()
    if not (args.fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {args.fixture / 'sphere.ini'}")
    if not args.binary.is_file():
        parser.error(f"server binary does not exist: {args.binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel

    failures: list[str] = []
    passed: list[str] = []
    returncode, runner_error, log_contents = run_server(
        fixture=args.fixture,
        binary=args.binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=args.fixture / "server.log",
        action=lambda: exercise(args, failures, passed),
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    total = 2
    if failures:
        print(f"dialog-argv probe failed: {len(passed)}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dialog-argv probe passed: {len(passed)}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
