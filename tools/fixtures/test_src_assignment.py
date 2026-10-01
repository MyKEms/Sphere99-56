#!/usr/bin/env python3
"""Check writable SRC, nested source scope, and an item @Timer source."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "ScriptGapsProbe"
LOGIN_VALUE = "script-gaps-pw"
MARKER = "SPHERE_SRC_ASSIGNMENT"
END_MARKER = MARKER + "_END"
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
    "tag_probe": "tag-probe",
    "caller_after": "SrcProbe",
    "timer_source": "SrcProbe",
    "timer_after": "SrcProbe",
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
    rows: dict[str, str] = {}
    for message in observed:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2)
    for key, expected in EXPECTED.items():
        value = rows.get(key)
        if value != expected:
            failures.append(f"{key}: got {value!r}; expected {expected!r}")
    # This row is a direct probe for the production ``TAG(combatTarget,<ACT>)``
    # form.  It is intentionally reported but not value-asserted here: the
    # same empty result is present on master and on this SRC-only change, so
    # that separate stock-parity gap must not be folded into this fix.
    if "tag_hit" not in rows:
        failures.append("TAG hit probe did not execute")
    if failures:
        print("SRC assignment probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"observed messages: {observed!r}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print(f"SRC assignment probe passed: {len(EXPECTED)}/{len(EXPECTED)} checks")
    print("SRC assignment rows:")
    for key in EXPECTED:
        print(f"- {key}={rows.get(key)}")
    print(f"- tag_hit_probe={rows['tag_hit']}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
