#!/usr/bin/env python3
"""Check that a missing numeric ARGV value evaluates to the stock zero."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.explevel_empty_argv import ACCOUNT, PASSWORD
from run_suite import shutdown_failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4630)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_gumps import parse_gump_dialog
    from uo_packets import split_packet_stream
    from uo_test_client import (
        decode_game_response,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    texts: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("empty-argv fixture account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("empty-argv fixture character did not enter the world")
            data = bytearray(response)
            seen_packets = 0
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                packets = split_packet_stream(
                    decode_game_response(bytes(data)), allow_truncated=True
                )
                for packet in packets[seen_packets:]:
                    seen_packets += 1
                    if packet.command in (0xB0, 0xDD):
                        texts[:] = list(parse_gump_dialog(packet.data).texts)
                        return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                data.extend(chunk)
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

    if not texts:
        failures.append("empty-argv probe did not receive its dialog")
    elif texts != ["0"]:
        failures.append(f"empty numeric ARGV value: got {texts!r}; expected ['0']")
    if failures:
        print("observed dialog texts:", texts, file=sys.stderr)
        print("explevel-empty-argv probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("explevel-empty-argv probe passed: empty numeric argument serialized as 0")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
