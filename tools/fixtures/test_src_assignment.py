#!/usr/bin/env python3
"""Check writable SRC, nested source scope, and an item @Timer source."""

from __future__ import annotations

import argparse
import json
import re
import socket
import struct
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "ScriptGapsProbe"
LOGIN_VALUE = "script-gaps-pw"
MARKER = "SPHERE_SRC_ASSIGNMENT"
END_MARKER = MARKER + "_END"
STEP_RESULT_PREFIX = f"{MARKER} step_result|"
MARKER_RE = re.compile(re.escape(MARKER) + r" ([a-z_]+)\|\[(.*)\]$")

EXPECTED = {
    "caller_before": "SrcProbe",
    "target_name": "synthetic SRC target",
    "target_serial": "040000005",
    "numeric_before": "SrcProbe",
    "numeric_inside": "synthetic SRC target",
    "literal_before": "SrcProbe",
    "literal_inside": "synthetic SRC target",
    "var_before": "SrcProbe",
    "var_inside": "synthetic SRC target",
    "missing_before": "SrcProbe",
    "missing_inside": "SrcProbe",
    "dynamic_before": "100",
    "dynamic_after": "111",
    "tag_probe": "tag-probe",
    "tag_hit": "040000005",
    "lastnew_before": "before",
    "lastnew_missing": "reached",
    "caller_after": "SrcProbe",
    "timer_source": "SrcProbe",
    "timer_after": "SrcProbe",
    "spell_effect_assigned": "040000005|040000005",
    "spell_effect_restored": "#00",
    "step_context": "SrcStepNpc|07|step-source",
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
    parser.add_argument("--port", type=int, default=2882)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--timer-timeout", type=float, default=20.0)
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

    observed: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("SRC assignment probe did not reach the character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("SRC assignment probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + args.timer_timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                observed[:] = system_messages(bytes(data))
                if END_MARKER in observed and any(
                    message.startswith(f"{MARKER} timer_after") for message in observed
                ):
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            observed[:] = system_messages(bytes(data))
            if END_MARKER in observed and any(
                message.startswith(f"{MARKER} timer_after") for message in observed
            ):
                # The stepped tile's handler changes SRC to a different NPC,
                # creates an item through that source, and fires LASTNEW's
                # trigger.  Two packets are used so the first establishes the
                # facing direction and the second crosses the tile.
                sock.sendall(struct.pack(">BBBI", 0x02, 2, 0, 0))
                sock.sendall(struct.pack(">BBBI", 0x02, 2, 1, 0))
                deadline = time.monotonic() + 5.0
                while time.monotonic() < deadline:
                    observed[:] = system_messages(bytes(data))
                    if any(message.startswith(STEP_RESULT_PREFIX) for message in observed):
                        break
                    try:
                        chunk = sock.recv(65536)
                    except socket.timeout:
                        continue
                    except OSError:
                        break
                    if not chunk:
                        break
                    data.extend(chunk)
                observed[:] = system_messages(bytes(data))
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
    if END_MARKER not in observed:
        failures.append("SRC assignment login did not reach its end marker")
    rows: dict[str, list[str]] = {}
    for message in observed:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows.setdefault(match.group(1), []).append(match.group(2))
    for key, expected in EXPECTED.items():
        values = rows.get(key, [])
        value = values[-1] if values else None
        if value != expected:
            failures.append(f"{key}: got {value!r}; expected {expected!r}")

    # Both trigger spellings must preserve the caller object.  The bare SRC
    # spelling is intentionally checked for presence rather than a fixed UID:
    # stock allocates a runtime UID while the synthetic fixture uses a stable
    # low serial.
    handlers = rows.get("trigger_handler", [])
    if len(handlers) != 2:
        failures.append(f"trigger_handler: got {handlers!r}; expected two rows")
    else:
        for expected_tag, value in zip(("explicit", "shorthand"), handlers):
            fields = value.split("|")
            if (
                len(fields) != 4
                or fields[0] != expected_tag
                or not fields[1]
                or fields[2] != "SrcProbe"
                or fields[3] != "03"
            ):
                failures.append(
                    f"trigger_handler {expected_tag}: got {value!r}; "
                    "expected non-empty SRC, SrcProbe, 03"
                )

    step_rows = rows.get("step_result", [])
    if len(step_rows) != 1:
        failures.append(f"step_result: got {step_rows!r}; expected one row")
    else:
        fields = step_rows[0].split("|")
        if len(fields) != 6:
            failures.append(f"step_result: got {step_rows[0]!r}; expected six fields")
        else:
            source_name, source_serial, source_tag, lastnew_serial, point, attr = fields
            if source_name != "SrcStepNpc":
                failures.append(f"step source name: got {source_name!r}; expected 'SrcStepNpc'")
            if not source_serial or source_serial.lstrip("0") != "7":
                failures.append(
                    f"step source serial: got {source_serial!r}; expected NPC serial 7"
                )
            if source_tag != "step-source":
                failures.append(
                    f"step source tag: got {source_tag!r}; expected 'step-source'"
                )
            if not lastnew_serial:
                failures.append("step LASTNEW serial was empty")
            if point != "130,128,0":
                failures.append(f"step LASTNEW point: got {point!r}; expected '130,128,0'")
            if not attr:
                failures.append("step LASTNEW ATTR was empty")

    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
    except (FileNotFoundError, OSError, json.JSONDecodeError) as error:
        failures.append(f"unknown-keyword report is unreadable: {error}")
    else:
        for entry in report.get("entries", []):
            if not isinstance(entry, dict):
                continue
            keyword = str(entry.get("keyword", "")).upper()
            if keyword.startswith("LASTNEW"):
                failures.append(f"missing LASTNEW root was reported as unknown: {entry!r}")
    if failures:
        print("SRC assignment probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"observed messages: {observed!r}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print(f"SRC assignment probe passed: {len(EXPECTED) + 3}/{len(EXPECTED) + 3} checks")
    print("SRC assignment rows:")
    for key in EXPECTED:
        print(f"- {key}={rows.get(key, [])[-1] if rows.get(key) else None}")
    print(f"- trigger_handler={rows.get('trigger_handler', [])}")
    print(f"- step_result={step_rows}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
