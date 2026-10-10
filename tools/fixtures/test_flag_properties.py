#!/usr/bin/env python3
"""Check the generic character FLAG_* property table."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.flag_properties import ACCOUNT, FLAGS, MARKER, PASSWORD
from run_suite import shutdown_failures


ROW_RE = re.compile(re.escape(MARKER) + r" ([a-z_]+)\|(.+)$")
EXPECTED_READ = tuple("0" for _ in FLAGS)
EXPECTED_SET = ("1", "1", "1", "1")
EXPECTED_CLEAR = ("0", "0", "0", "0")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def _values(payload: str) -> tuple[str, ...]:
    return tuple(value for value in re.findall(r"\[([^\]]*)\]", payload))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=5200)
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
            raise RuntimeError("flag-properties account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("flag-properties character did not enter the world")
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
            rows[match.group(1)] = _values(match.group(2))
    expected = {
        "read": EXPECTED_READ,
        "set": EXPECTED_SET,
        "clear": EXPECTED_CLEAR,
    }
    for key, want in expected.items():
        got = rows.get(key)
        if got != want:
            failures.append(f"{key}: got {got!r}; expected {want!r}")
    if f"{MARKER}_END" not in messages:
        failures.append("flag-properties probe did not reach its end marker")

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print(
            f"flag-properties probe failed: {max(0, len(expected) - len(failures))}/{len(expected)} rows passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"flag-properties probe passed: {len(expected)}/{len(expected)} rows")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
