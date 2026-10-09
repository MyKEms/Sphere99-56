#!/usr/bin/env python3
"""Check a dotted property chain rooted at a TAG-held item UID."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.explevel_object_chain import ACCOUNT, END_MARKER, MARKER, PASSWORD
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
    parser.add_argument("--port", type=int, default=4600)
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
            raise RuntimeError("object-chain probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("object-chain probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
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

    rows = [message for message in messages if message.startswith(MARKER + " ")]
    if len(rows) != 1:
        failures.append(f"object-chain marker count {len(rows)} != 1")
    else:
        fields = rows[0][len(MARKER) + 2 : -1].split("|")
        if (
            len(fields) != 7
            or fields[1] != "1"
            or fields[0] != fields[3]
            or fields[2] != fields[4]
            or fields[2] != "synthetic explevel weapon"
            or fields[5] != "T_EQ_SCRIPT"
            or fields[6] != "SYNTHETIC_EXPLEVEL_WEAPON"
        ):
            failures.append(f"object-chain marker fields {fields!r} do not identify the equipped item")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("explevel-object-chain probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("explevel-object-chain probe passed: TAG-held item chain resolved")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
