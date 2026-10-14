#!/usr/bin/env python3
"""Check nested region precedence, plane matching, and reversed rectangles."""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

from modes.region_shadowing import ACCOUNT, MARKER, PASSWORD
from run_suite import shutdown_failures


ROW_RE = re.compile(r"^" + re.escape(MARKER) + r" ([a-z_]+)\|\[(.*?)\]\|\[(.*?)\]\|\[(.*?)\]$")
END_MARKER = MARKER + "_END"
EXPECTED = {
    "inside": ("SYNTHETIC INNER", "02000", "300,300,0"),
    "plane": ("SYNTHETIC OTHER PLANE", "04000", "300,300,0,100"),
    "reversed": ("SYNTHETIC REVERSED", "08000", "400,400,0"),
    "outside": ("SYNTHETIC WORLD", "01000", "320,320,0"),
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
    parser.add_argument("--port", type=int, default=43209)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("region-shadowing probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("region-shadowing character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except TimeoutError:
                    continue
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    rows: dict[str, tuple[str, str, str]] = {}
    for message in messages:
        match = ROW_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = (match.group(2), match.group(3), match.group(4))
    for name, expected in EXPECTED.items():
        if rows.get(name) != expected:
            failures.append(f"{name}: got {rows.get(name)!r}; expected {expected!r}")
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")

    if failures:
        print("region-shadowing probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("observed messages:", messages, file=sys.stderr)
        return 1
    print(f"region-shadowing probe passed: {len(EXPECTED) + 1}/{len(EXPECTED) + 1} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
