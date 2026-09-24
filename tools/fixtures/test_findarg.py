#!/usr/bin/env python3
"""Verify resource-reference event add, deduplication, and removal."""

from __future__ import annotations

import argparse
import re
import sys
import time
from pathlib import Path

from make_fixture import FINDARG_ACCOUNT, FINDARG_MARKER
from run_suite import shutdown_failures


LOGIN_VALUE = "findarg-probe-pw"
END_MARKER = FINDARG_MARKER + "_END"
ROW_RE = re.compile(re.escape(FINDARG_MARKER) + r" (after_add|after_remove)\|\[(.*)\]$")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
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
    parser.add_argument("--port", type=int, default=2734)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server, wait_for_saved_pair
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    messages: list[str] = []
    saved_chars = ""
    failures: list[str] = []

    def exercise() -> None:
        nonlocal saved_chars
        sock, _ = game_connect(
            args.host,
            args.port,
            FINDARG_ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("FindArg probe did not reach the character list")
        try:
            sock.sendall(
                make_char_create(
                    name=FINDARG_ACCOUNT,
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
                raise RuntimeError("FindArg probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 20.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    break
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
        saved_chars, _ = wait_for_saved_pair(fixture, 30.0)

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

    rows = {
        match.group(1): match.group(2)
        for message in messages
        if (match := ROW_RE.fullmatch(message))
    }
    if END_MARKER not in messages:
        failures.append("FindArg probe did not reach its bounded end marker")

    after_add = rows.get("after_add")
    after_remove = rows.get("after_remove")
    if after_add is None:
        failures.append("FindArg probe did not emit its after_add row")
    if after_remove is None:
        failures.append("FindArg probe did not emit its after_remove row")
    add_values = after_add.split(",") if after_add else []
    remove_values = after_remove.split(",") if after_remove else []
    if add_values.count("e_FindArgProbe") != 1:
        failures.append(
            "duplicate event add was not collapsed: "
            f"after_add={after_add!r}"
        )
    if "e_FindArgProbe" in remove_values:
        failures.append(
            "EVENTS -e_FindArgProbe left the event attached: "
            f"after_remove={after_remove!r}"
        )
    if re.search(r"(?m)^EVENTS=e_FindArgProbe$", saved_chars):
        failures.append("saved character still contains e_FindArgProbe after removal")
    if saved_chars and saved_chars.count("EVENTS=e_FindArgProbe") != 0:
        failures.append("saved character serialized a removed FindArg event")

    if failures:
        print("FindArg probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(
        "FindArg probe passed: duplicate add collapsed, named removal cleared "
        "the event, and saved output stayed clean"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
