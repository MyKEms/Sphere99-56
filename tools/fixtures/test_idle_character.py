#!/usr/bin/env python3
"""Check bounded idle-character status ticks and stable regeneration."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.idle_character import ACCOUNT, PASSWORD
from run_suite import shutdown_failures


IDLE_RE = re.compile(r"^SPHERE_IDLE\|(-?\d+)\|(-?\d+)\|(-?\d+)\|(-?\d+)$")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def wait_for_world_load(log_path: Path, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            text = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            text = ""
        if "world load:" in text:
            return
        time.sleep(0.1)
    raise RuntimeError("idle-character world did not finish loading before timeout")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3190)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--sample-seconds", type=float, default=12.0)
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
        wait_for_world_load(fixture / "server.log", args.startup_timeout)
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("idle-character account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("idle-character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + args.sample_seconds
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
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

    try:
        returncode, runner_error, log_contents = run_server(
            fixture=fixture,
            binary=binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=fixture / "server.log",
            action=exercise,
        )
    except (OSError, RuntimeError, ValueError) as error:
        returncode, runner_error, log_contents = None, str(error), ""
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    rows = [
        tuple(map(int, match.groups()))
        for message in messages
        if (match := IDLE_RE.fullmatch(message))
    ]
    if messages.count("SPHERE_RATE_SET") != 1:
        failures.append(
            f"login marker count {messages.count('SPHERE_RATE_SET')} != 1"
        )
    if len(rows) < 4:
        failures.append(f"only {len(rows)} idle samples arrived; expected at least 4")
    if rows:
        first_food = rows[0][0]
        if any(food != first_food for food, _, _, _ in rows):
            failures.append(f"FOOD changed during the bounded idle probe: {rows!r}")
        if any(mana != 100 for _, _, mana, _ in rows):
            failures.append(f"MANA changed during the bounded idle probe: {rows!r}")
        if any(stam != 100 for _, _, _, stam in rows):
            failures.append(f"STAM changed during the bounded idle probe: {rows!r}")
        hits = [hit for _, hit, _, _ in rows]
        if any(new < old for old, new in zip(hits, hits[1:])):
            failures.append(f"HITS regressed during the bounded idle probe: {hits!r}")
        if max(hits) != 100:
            failures.append(f"HITS did not reach the saved character's max: {hits!r}")

    if failures:
        print(f"idle-character probe failed ({len(failures)} failure(s))", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"observed messages: {messages!r}", file=sys.stderr)
        return 1
    print(f"idle-character probe passed: {len(rows)} status samples")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
