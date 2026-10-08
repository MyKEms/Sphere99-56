#!/usr/bin/env python3
"""Check that EQUIP accepts a live LASTNEW reference and dispatches @Equip."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.light_equip_reference import ACCOUNT, MARKER, PASSWORD, READY_MARKER
from run_suite import shutdown_failures


MARKER_RE = re.compile(re.escape(MARKER) + r" ([0-9]+)\|([0-9]+)\|([0-9]+)$")


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
    parser.add_argument("--port", type=int, default=3158)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("light-equip reference account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("light-equip reference character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if f"{MARKER}_END" in messages:
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
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    ready_count = sum(message.startswith(READY_MARKER + " ") for message in messages)
    if ready_count != 1:
        failures.append(f"ready marker count {ready_count} != 1")
    rows = [message for message in messages if message.startswith(MARKER + " ")]
    if len(rows) != 1:
        failures.append(f"equip marker count {len(rows)} != 1")
    else:
        match = MARKER_RE.fullmatch(rows[0])
        if match is None:
            failures.append(f"malformed equip marker: {rows[0]!r}")
        elif match.group(1) != "1":
            failures.append(f"nightsight value {match.group(1)!r} != '1'")
    if messages.count(f"{MARKER}_END") != 1:
        failures.append(f"end marker count {messages.count(f'{MARKER}_END')} != 1")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("light-equip reference probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("light-equip reference probe passed: live LASTNEW reference dispatched @Equip once")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
