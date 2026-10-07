#!/usr/bin/env python3
"""Check that a full 0.99 resource UID selects its character definition."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures
from modes.resource_char_uid import ACCOUNT, MARKER, PASSWORD


END_MARKER = MARKER + "_END"
MARKER_RE = re.compile(re.escape(MARKER) + r"\s+([^|]+)\|([0-9]+)$")
EXPECTED_NAME = "resource UID fixture character"
EXPECTED_STR = "100"


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
    parser.add_argument("--port", type=int, default=3002)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import decode_game_response, find_start_packet, game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("resource-UID probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("resource-UID probe character did not enter the world")
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
    if "WORLDCHAR load fallback:" in log_contents:
        failures.append("resource-UID character fell back to DEFAULTCHAR")
    if "defaulted=1" in log_contents:
        failures.append("world load diagnostics recorded a defaulted character")
    if END_MARKER not in messages:
        failures.append("resource-UID probe did not reach its end marker within the bounded timeout")

    rows: list[tuple[str, str]] = []
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows.append((match.group(1), match.group(2)))
    if (EXPECTED_NAME, EXPECTED_STR) not in rows:
        failures.append(
            f"resource definition marker: got {rows!r}; expected {(EXPECTED_NAME, EXPECTED_STR)!r}"
        )

    if failures:
        print("observed messages:", messages, file=sys.stderr)
        print("resource-char-uid probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("resource-char-uid probe passed: 3/3 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
