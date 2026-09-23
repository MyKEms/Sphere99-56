#!/usr/bin/env python3
"""Check Sphere's 0-prefixed hexadecimal DWORD resource properties.

The fixture comes from ``make_fixture.py --dword-hex-probe``.  It reads two
synthetic CHARDEF ``ANIM`` properties through ``FINDUID(...).ANIM`` so the
test covers both resource loading and the script-facing property getter.
"""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import DWORD_HEX_ACCOUNT, DWORD_HEX_MARKER
from run_suite import shutdown_failures


LOGIN_VALUE = "dword-hex-probe-pw"
END_MARKER = DWORD_HEX_MARKER + " C_END"
MARKER_RE = re.compile(
    re.escape(DWORD_HEX_MARKER) + r" C\|([a-z0-9_]+)\|\[(.*)\]$"
)

EXPECTED = {
    "high": "0ffc78c7f",
    "low": "03fbc7f",
}


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def parse_rows(messages: list[str]) -> dict[str, str]:
    rows: dict[str, str] = {}
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2)
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2734)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            DWORD_HEX_ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("DWORD-hex probe account did not reach the character list")
        try:
            sock.sendall(
                make_char_create(
                    name=DWORD_HEX_ACCOUNT,
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
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("DWORD-hex probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
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

    if END_MARKER not in messages:
        failures.append("DWORD-hex probe did not reach its end marker within the bounded timeout")
    rows = parse_rows(messages)
    for key, expected in EXPECTED.items():
        value = rows.get(key)
        if value != expected:
            failures.append(f"{key}: got {value!r}; expected {expected!r}")

    if failures:
        print(
            f"DWORD-hex probe failed: {len(EXPECTED) - len(failures)}/{len(EXPECTED)} checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"DWORD-hex probe passed: {len(EXPECTED)}/{len(EXPECTED)} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
