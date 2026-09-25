#!/usr/bin/env python3
"""Check 0.99 region weather keys on the sectors they cover.

The fixture seeds an existing character at (128,128), loads a small area with
RAINCHANCE and COLDCHANCE, and reads the values back through SRC.SECTOR.  It
also checks that the unknown-keyword report has no weather-key entries.
"""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import (
    REGION_WEATHER_ACCOUNT,
    REGION_WEATHER_COLD,
    REGION_WEATHER_MARKER,
    REGION_WEATHER_RAIN,
)
from run_suite import shutdown_failures


LOGIN_VALUE = "region-pw"
END_MARKER = REGION_WEATHER_MARKER + "_END"
MARKER_RE = re.compile(
    re.escape(REGION_WEATHER_MARKER) + r" ([0-9]+)\|([0-9]+)$"
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
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


def weather_report_failures(path: Path) -> list[str]:
    try:
        report = json.loads(path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        return []
    except (OSError, json.JSONDecodeError) as error:
        return [f"unknown-keyword report is unreadable: {error}"]
    entries = report.get("entries", [])
    failures = []
    for entry in entries:
        if not isinstance(entry, dict):
            continue
        keyword = str(entry.get("keyword", "")).upper()
        if keyword.startswith(("RAINCHANCE", "COLDCHANCE")):
            failures.append(
                "region weather key was reported as unknown: "
                f"{entry.get('keyword')!r}"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2750)
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
        make_char_play,
        recv_until_game_start,
    )

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        wait_for_world_load(fixture / "server.log")
        sock, _ = game_connect(
            args.host,
            args.port,
            REGION_WEATHER_ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("region-weather probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("region-weather probe character did not enter the world")
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
        failures.append("region-weather probe did not reach its end marker")

    rows = {}
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows["weather"] = (int(match.group(1)), int(match.group(2)))
    expected = (REGION_WEATHER_RAIN, REGION_WEATHER_COLD)
    if rows.get("weather") != expected:
        failures.append(f"sector weather: got {rows.get('weather')!r}; expected {expected!r}")
    failures.extend(weather_report_failures(fixture / "logs" / "unknown-keywords.json"))

    if failures:
        print(f"region-weather probe failed: {len(failures)} failure(s)", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("region-weather probe passed: sector weather and unknown-keyword checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
