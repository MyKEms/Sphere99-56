#!/usr/bin/env python3
"""Verify that inline TEXTA controls become ordinary text controls."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from make_texta_fixture import ACCOUNT, ITEM_SERIAL, PASSWORD
from run_suite import shutdown_failures


EXPECTED_RANGES = ("(80 - 120)", "(60 - 90)", "(20 - 50)", "(80 - 130)")
EXPECTED_POSITIONS = (233, 269, 305, 341)


def _packets(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str]) -> None:
    from uo_gumps import parse_gump_dialog
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        args.host,
        args.port,
        ACCOUNT,
        PASSWORD,
        game_port=args.port + 1000,
    )
    if sock is None:
        raise RuntimeError("TEXTA probe account did not reach the character list")
    raw = bytearray()
    try:
        sock.sendall(make_char_play(0))
        raw.extend(recv_until_game_start(sock, timeout=30.0))
        if find_start_packet(decode_game_response(bytes(raw))) is None:
            raise RuntimeError("TEXTA probe character did not enter the world")

        # The saved item is next to the character, so this exercises the same
        # dialog path as a client double-click on the fixture object.
        sock.sendall(bytes((0x06,)) + ITEM_SERIAL.to_bytes(4, "big"))
        deadline = time.monotonic() + 10.0
        gump = None
        while time.monotonic() < deadline and gump is None:
            packets = _packets(bytes(raw))
            for packet in packets:
                if packet.command != 0xB0:
                    continue
                try:
                    gump = parse_gump_dialog(packet.data)
                except ValueError as error:
                    failures.append(f"malformed gump packet: {error}")
                    return
                break
            if gump is not None:
                break
            sock.settimeout(0.2)
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            raw.extend(chunk)

        if gump is None:
            failures.append("TEXTA dialog did not arrive")
            return
        controls = list(gump.controls)
        text_controls = [control for control in controls if control.kind.casefold() == "text"]
        raw_texta = [control for control in controls if control.kind.casefold() == "texta"]
        if raw_texta:
            failures.append(f"raw TEXTA controls reached the packet: {len(raw_texta)}")
        else:
            passed.append("no raw TEXTA controls")
        if len(text_controls) != 4:
            failures.append(f"expected four visible text controls, got {len(text_controls)}")
            return
        if any(control.kind.casefold() != "text" for control in text_controls):
            failures.append("inline controls were not normalized to Text")
        else:
            passed.append("four visible range controls")

        positions = tuple(int(control.args[1]) for control in text_controls)
        if positions != EXPECTED_POSITIONS:
            failures.append(f"range control positions {positions!r}, expected {EXPECTED_POSITIONS!r}")
        else:
            passed.append("range control positions")
        text_ids = tuple(int(control.args[3]) for control in text_controls)
        if text_ids != tuple(range(4)):
            failures.append(f"range text ids {text_ids!r}, expected 0..3")
        ranges = tuple(gump.texts[text_id] for text_id in text_ids if 0 <= text_id < len(gump.texts))
        if ranges != EXPECTED_RANGES:
            failures.append(f"range texts {ranges!r}, expected {EXPECTED_RANGES!r}")
        else:
            passed.append("four visible range texts")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2992)
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
    from test_world_save_roundtrip import run_server

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
    if failures:
        print(f"TEXTA probe failed: {len(passed)} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"TEXTA probe passed: {len(passed)} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
