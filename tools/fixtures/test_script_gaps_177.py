#!/usr/bin/env python3
"""Check ACT.P, DEFNAME UID roots, and CHARDEF flag assignments."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures
from modes.script_gaps_177 import ACCOUNT, MARKER, PASSWORD


ROW_RE = re.compile(
    re.escape(MARKER)
    + r" ([a-z0-9_]+)\|\[(.*?)\](?:\|\[(.*?)\])?(?:\|\[(.*?)\])?$"
)
EXPECTED = {
    "act_after": ("131", "131", "0"),
    "uid_name": ("synthetic container",),
    "uid_tag": ("177",),
    "flags": ("0", "1"),
}


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
    parser.add_argument("--port", type=int, default=2898)
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
            raise RuntimeError("script-gaps 177 account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("script-gaps 177 character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
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

    rows: dict[str, tuple[str, ...]] = {}
    for message in messages:
        match = ROW_RE.fullmatch(message)
        if match:
            values = tuple(value or "" for value in match.groups()[1:] if value is not None)
            rows[match.group(1)] = values
    for key, expected in EXPECTED.items():
        value = rows.get(key)
        if value != expected:
            failures.append(f"{key}: got {value!r}; expected {expected!r}")
    if f"{MARKER}_END" not in messages:
        failures.append("script-gaps 177 probe did not reach its end marker")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print(
            f"script-gaps 177 probe failed: {max(0, len(EXPECTED) - len(failures))}/{len(EXPECTED)} rows passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"script-gaps 177 probe passed: {len(EXPECTED)}/{len(EXPECTED)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
