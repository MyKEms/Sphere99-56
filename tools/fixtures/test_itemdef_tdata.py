#!/usr/bin/env python3
"""Check that item definition TDATA keeps resource names."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.itemdef_tdata import ACCOUNT, BOTTLE_NAME, LOGIN_TOKEN, MARKER
from run_suite import shutdown_failures


TDATA_RE = re.compile(re.escape(MARKER) + r" tdata1 \[(.*)\] tdata2 \[(.*)\] tdata3 \[(.*)\] tdata4 \[(.*)\]$")


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
    parser.add_argument("--port", type=int, default=2998)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(args.host, args.port, ACCOUNT, LOGIN_TOKEN, game_port=args.port + 1000)
        if sock is None:
            raise RuntimeError("item TDATA probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("item TDATA probe character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
                if f"{MARKER} done" in messages:
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
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    def value(text: str) -> int:
        try:
            return int(text, 16) if text else 0
        except ValueError:
            return 0

    tdata = next((TDATA_RE.search(m) for m in messages if TDATA_RE.search(m)), None)
    if tdata is None:
        failures.append("TDATA values were not reported")
    else:
        named, numeric, char, small = (value(group) for group in tdata.groups())
        if named == 0:
            failures.append(f"TDATA1 naming an item definition loaded as zero ({tdata.group(1)!r})")
        if char == 0:
            failures.append(f"TDATA3 naming a character definition loaded as zero ({tdata.group(3)!r})")
        if numeric & 0xFFFF != 0x0F0E:
            failures.append(f"numeric TDATA2 changed ({tdata.group(2)!r})")
        if small != 5:
            failures.append(f"numeric TDATA4 changed ({tdata.group(4)!r})")
    for label in ("named", "numeric"):
        if f"{MARKER} {label} [{BOTTLE_NAME}]" not in messages:
            failures.append(f"NEWITEM from the {label} TDATA did not create the referenced item")
    if f"{MARKER} done" not in messages:
        failures.append("item TDATA probe did not finish")

    if failures:
        print("item TDATA probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("item TDATA probe passed: named and numeric TDATA values resolve")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
