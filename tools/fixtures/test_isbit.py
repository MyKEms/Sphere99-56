#!/usr/bin/env python3
"""Check the 0.99 ISBIT(value, bit-position) script function."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import ISBIT_ACCOUNT, ISBIT_MARKER, ISBIT_PASSWORD
from run_suite import shutdown_failures


END_MARKER = ISBIT_MARKER + "_END"
MARKER_RE = re.compile(re.escape(ISBIT_MARKER) + r" C\|bits\|(.+)$")
EXPECTED = "1|0|1|1|0"


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def wait_for_world_load(log_path: Path, timeout: float = 60.0) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            log = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            log = ""
        if "world load:" in log:
            return
        time.sleep(0.1)
    raise RuntimeError("server did not finish world load before the bounded timeout")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2760)
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
        wait_for_world_load(fixture / "server.log")
        sock, _ = game_connect(
            args.host,
            args.port,
            ISBIT_ACCOUNT,
            ISBIT_PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("ISBIT probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("ISBIT probe character did not enter the world")
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
        failures.append("ISBIT probe did not reach its end marker within the bounded timeout")
    values = [match.group(1) for message in messages if (match := MARKER_RE.fullmatch(message))]
    if values != [EXPECTED]:
        failures.append(f"bits: got {values!r}; expected {[EXPECTED]!r}")

    total = 2
    if failures:
        print(f"ISBIT probe failed: {max(0, total - len(failures))}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"ISBIT probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
