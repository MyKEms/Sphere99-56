#!/usr/bin/env python3
"""Check that a skill-made plain item reaches the make-item trigger."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.nonvendable_makeitem import ACCOUNT, LOGIN_VALUE, MARKER
from run_suite import shutdown_failures

SUCCESS_RE = re.compile(
    re.escape(MARKER) + r"\|success\|uid=(\S+)\|type=(\S*)$"
)


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
    parser.add_argument("--port", type=int, default=43213)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("non-vendable craft probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("non-vendable craft character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
                if f"{MARKER}|done" in messages:
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    if messages.count(f"{MARKER}|started") != 1:
        failures.append(
            f"started marker count {messages.count(f'{MARKER}|started')} != 1"
        )
    success = [message for message in messages if f"{MARKER}|success|" in message]
    if len(success) != 1:
        failures.append(f"success marker count {len(success)} != 1")
    else:
        match = SUCCESS_RE.fullmatch(success[0])
        if match is None:
            failures.append(f"malformed success marker: {success[0]!r}")
        else:
            uid, item_type = match.groups()
            if int(uid.lstrip("0"), 16) == 0:
                failures.append(f"crafted item UID was empty: {uid!r}")
            if item_type.upper() != "T_NORMAL":
                failures.append(f"crafted item type was {item_type!r}, expected T_NORMAL")
    if messages.count(f"{MARKER}|done") != 1:
        failures.append(f"done marker count {messages.count(f'{MARKER}|done')} != 1")

    if failures:
        print("non-vendable craft probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("non-vendable craft probe passed: plain crafted item reached the trigger")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
