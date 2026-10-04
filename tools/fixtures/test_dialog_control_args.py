"""Check that dialog numeric arguments are evaluated before packet encoding."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from modes.dialog_control_args import ACCOUNT, PASSWORD
from run_suite import shutdown_failures


EXPECTED = [
    "gumppic 30 210 12674",
    "button 160 330 2440 2440 0 1 0",
    "button 25 395 4005 4007 1 0 1",
    "gumppic 50 420 15",
]


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str]) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_packets import (  # pylint: disable=import-outside-toplevel
        gump_dialog_controls,
        parse_gump_dialog,
        split_packet_stream,
    )
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    # The fixture runner's -P value is the advertised game endpoint.  Keep
    # the test on that endpoint instead of assuming the administrative socket
    # is the relay target.
    sock, _ = game_connect(args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port)
    if sock is None:
        raise RuntimeError("dialog control-args account did not reach the character list")
    try:
        sock.sendall(
            make_char_create(
                name=ACCOUNT,
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
            raise RuntimeError("dialog control-args character did not enter the world")
        seen = 0

        def wait_for(match: Callable[[bytes], bool], timeout: float = 10.0) -> Optional[bytes]:
            nonlocal seen
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                packets = split_packet_stream(
                    decode_game_response(bytes(raw)), allow_truncated=True
                )
                for index in range(seen, len(packets)):
                    if match(packets[index].data):
                        seen = index + 1
                        return packets[index].data
                seen = len(packets)
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    return None
                raw.extend(chunk)
            return None

        gump = wait_for(lambda data: parse_gump_dialog(data) is not None)
        if gump is None:
            failures.append("dialog was not opened")
            return
        controls = [" ".join(control.split()) for control in gump_dialog_controls(gump)]
        if controls != EXPECTED:
            failures.append(f"dialog controls {controls!r}; expected {EXPECTED!r}")
        else:
            passed.append("bare and ARGO numeric controls")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3140)
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

    if failures:
        print(
            f"dialog-control-args probe failed: {len(passed)}/1 checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dialog-control-args probe passed: {len(passed)}/1 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
