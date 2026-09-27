#!/usr/bin/env python3
"""Verify that a bounded script loop leaves the server responsive."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
from pathlib import Path

from make_fixture import RUNAWAY_LOOP_LIMIT, RUNAWAY_LOOP_MARKER
from run_suite import shutdown_failures, stop_server, wait_for_port


ACCOUNT_ONE = "RunawayLoopOne"
ACCOUNT_TWO = "RunawayLoopTwo"
PASSWORD = "runlpw"
LOOP_LOG_RE = re.compile(
    rf"WHILE loop stopped after {RUNAWAY_LOOP_LIMIT} iterations"
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages: list[str] = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0]
        messages.append(text.decode("ascii", errors="replace"))
    return messages


def enter_world(host: str, port: int, game_port: int, account: str) -> tuple[bytes, str]:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(host, port, account, PASSWORD, game_port=game_port)
    if sock is None:
        raise RuntimeError(f"{account} did not reach its character list")
    try:
        sock.sendall(make_char_create(name=account, start_loc=1))
        response = decode_game_response(recv_until_game_start(sock, timeout=20.0))
    finally:
        sock.close()
    if find_start_packet(response) is None:
        raise RuntimeError(f"{account} did not receive a valid game-entry packet")
    return response, account


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2754)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    log_path = fixture / "server.log"
    failures: list[str] = []
    returncode: int | None = None
    responses: list[bytes] = []

    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open server log: {error}", file=sys.stderr)
        return 1

    with log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            for account in (ACCOUNT_ONE, ACCOUNT_TWO):
                response, _ = enter_world(
                    args.host, args.port, args.port + 1000, account
                )
                responses.append(response)
        except (OSError, RuntimeError, socket.timeout) as error:
            failures.append(str(error))
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")

    failures.extend(shutdown_failures(returncode, log_contents))
    for account, response in zip((ACCOUNT_ONE, ACCOUNT_TWO), responses):
        messages = system_messages(response)
        if RUNAWAY_LOOP_MARKER + "_RETURNED" not in messages:
            failures.append(f"{account} did not reach the post-loop marker")
    if responses and f"{RUNAWAY_LOOP_MARKER}_CONFIG {RUNAWAY_LOOP_LIMIT}" not in system_messages(responses[0]):
        failures.append("first client did not observe SCRIPTLOOPLIMIT=32")
    if len(responses) != 2:
        failures.append("the second client did not receive a game response")
    loop_reports = LOOP_LOG_RE.findall(log_contents)
    if len(loop_reports) != 1:
        failures.append(
            f"expected one bounded loop diagnostic, found {len(loop_reports)}"
        )
    if f"after {RUNAWAY_LOOP_LIMIT} iterations" not in log_contents:
        failures.append("configured loop limit was not reported")
    if f"after {RUNAWAY_LOOP_LIMIT * 2} iterations" in log_contents:
        failures.append("loop diagnostic appears to report a cumulative limit")

    total = 7
    passed = max(0, total - len(failures))
    if failures:
        print(f"runaway-loop probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print(f"runaway-loop probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
