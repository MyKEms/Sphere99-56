#!/usr/bin/env python3
"""Check character FOOD and the default object used by an item event."""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import FOOD_ACCOUNT, FOOD_MARKER, FOOD_PASSWORD
from run_suite import shutdown_failures


END_MARKER = FOOD_MARKER + "_END"
MARKER_RE = re.compile(re.escape(FOOD_MARKER) + r" ([A-Z]+)\|(-?\d+)\|(-?\d+)$")


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


def unknown_food_failures(path: Path) -> list[str]:
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return ["unknown-keyword report is missing"]
    except (OSError, json.JSONDecodeError) as error:
        return [f"unknown-keyword report is unreadable: {error}"]
    entries = report.get("entries", [])
    food_entries = [
        entry for entry in entries
        if isinstance(entry, dict) and str(entry.get("keyword", "")).upper() == "FOOD"
    ]
    if food_entries:
        return [f"FOOD remained unresolved: {food_entries!r}"]
    return []


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2762)
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
            FOOD_ACCOUNT,
            FOOD_PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("FOOD probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("FOOD probe character did not enter the world")
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
        failures.append("FOOD probe did not reach its end marker within the bounded timeout")

    rows: dict[str, tuple[int, int]] = {}
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = (int(match.group(2)), int(match.group(3)))

    # The character's stat path is valid in both contexts.  The item trigger
    # deliberately uses the item as its default object; 0.99 treats its bare
    # FOOD access as the source character's hunger stat.
    if rows.get("C") != (7, 7):
        failures.append(f"character FOOD: got {rows.get('C')!r}; expected (7, 7)")
    if rows.get("ITEM") != (0, 0):
        failures.append(f"item default object: got {rows.get('ITEM')!r}; expected (0, 0)")
    failures.extend(unknown_food_failures(fixture / "logs" / "unknown-keywords.json"))

    total = 3
    if failures:
        print(f"FOOD probe failed: {max(0, total - len(failures))}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"FOOD probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
