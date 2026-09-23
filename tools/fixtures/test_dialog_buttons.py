#!/usr/bin/env python3
"""Press dialog buttons and check which BUTTON-section entry runs.

The fixture comes from ``make_fixture.py --dialog-button-probe``.  Its login
trigger opens a dialog whose BUTTON section lists ``ON=@anybutton`` first,
then ``ON=<numbered>`` and ``ON=0``.  A numbered button and cancel must run
their own entries; any other button falls back to ``ON=@anybutton``.  Every
handler reports ``ARGN``; the fallback also reports the checked switch, the
text entry and ``ARGO``, and stores a TAG that the cancel handler reads back.
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from make_fixture import (
    DIALOG_BUTTON_ACCOUNT,
    DIALOG_BUTTON_FALLBACK,
    DIALOG_BUTTON_MARKER,
    DIALOG_BUTTON_NUMBERED,
    DIALOG_BUTTON_SWITCH,
    DIALOG_BUTTON_TEXT_ID,
)
from run_suite import shutdown_failures


LOGIN_VALUE = "dialog-button-probe-pw"
TEXT_VALUE = "probe text"
MARKER_PREFIX = DIALOG_BUTTON_MARKER + " "

# (button, switches, texts, expected handler reports in order)
PRESSES = (
    (
        DIALOG_BUTTON_NUMBERED,
        (),
        (),
        f"numbered|{DIALOG_BUTTON_NUMBERED}",
    ),
    (
        DIALOG_BUTTON_FALLBACK,
        (DIALOG_BUTTON_SWITCH,),
        ((DIALOG_BUTTON_TEXT_ID, TEXT_VALUE),),
        f"any|{DIALOG_BUTTON_FALLBACK}|1|{TEXT_VALUE}|{DIALOG_BUTTON_ACCOUNT}",
    ),
    (
        0,
        (),
        (),
        f"cancel|0|any/{DIALOG_BUTTON_FALLBACK}",
    ),
)


def system_message(data: bytes) -> Optional[str]:
    if data[0] != 0x1C or len(data) < 45:
        return None
    return data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str]) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    # pylint: disable=import-outside-toplevel
    from uo_packets import make_gump_reply, parse_gump_dialog, split_packet_stream
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        args.host, args.port, DIALOG_BUTTON_ACCOUNT, LOGIN_VALUE, game_port=args.port + 1000
    )
    if sock is None:
        raise RuntimeError("probe account did not reach the character list")
    try:
        sock.sendall(
            make_char_create(
                name=DIALOG_BUTTON_ACCOUNT,
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
            raise RuntimeError("probe character did not enter the world")
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
        for button, switches, texts, expected in PRESSES:
            if gump is None:
                failures.append(f"no dialog was open for button {button}")
                return
            serial, context = parse_gump_dialog(gump)
            sock.sendall(make_gump_reply(serial, context, button, switches, texts))
            report = wait_for(
                lambda data: (system_message(data) or "").startswith(MARKER_PREFIX)
            )
            report = None if report is None else system_message(report)[len(MARKER_PREFIX):]
            if report != expected:
                failures.append(f"button {button} ran {report!r}; expected {expected!r}")
            else:
                passed.append(f"button {button}")
            if button:
                gump = wait_for(lambda data: parse_gump_dialog(data) is not None)

        # Nothing else reports: a numbered or cancel press ran one entry only.
        extra = wait_for(
            lambda data: (system_message(data) or "").startswith(MARKER_PREFIX), timeout=1.0
        )
        if extra is not None:
            failures.append(f"unexpected extra report {system_message(extra)!r}")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2734)
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

    total = len(PRESSES)
    if failures:
        print(f"dialog-button probe failed: {len(passed)}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dialog-button probe passed: {len(passed)}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
