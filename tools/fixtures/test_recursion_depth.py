#!/usr/bin/env python3
"""Verify that recursive functions and triggers cannot exhaust the stack."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
from pathlib import Path

from make_fixture import (
    RECURSION_DEPTH_ACCOUNT_ONE,
    RECURSION_DEPTH_ACCOUNT_TWO,
    RECURSION_DEPTH_MARKER,
    RECURSION_DEPTH_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


RECURSION_ERROR = re.compile(r"Trigger Recursion error !")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages: list[str] = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0]
        messages.append(text.decode("ascii", errors="replace"))
    return messages


def enter_world(host: str, port: int, account: str) -> bytes:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        host,
        port,
        account,
        RECURSION_DEPTH_PASSWORD,
        game_port=port + 1000,
    )
    if sock is None:
        raise RuntimeError(f"{account} did not reach its character list")
    try:
        sock.sendall(make_char_create(name=account, start_loc=1))
        response = decode_game_response(recv_until_game_start(sock, timeout=30.0))
    finally:
        sock.close()
    if find_start_packet(response) is None:
        raise RuntimeError(f"{account} did not receive a valid game-entry packet")
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2802)
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
    responses: list[bytes] = []
    returncode: int | None = None

    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            for account in (RECURSION_DEPTH_ACCOUNT_ONE, RECURSION_DEPTH_ACCOUNT_TWO):
                responses.append(enter_world(args.host, args.port, account))
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
    for account, response in zip(
        (RECURSION_DEPTH_ACCOUNT_ONE, RECURSION_DEPTH_ACCOUNT_TWO), responses
    ):
        messages = system_messages(response)
        if RECURSION_DEPTH_MARKER + "_FUNCTION_RETURNED" not in messages:
            failures.append(f"{account} did not return from recursive function")
        if RECURSION_DEPTH_MARKER + "_TRIGGER_RETURNED" not in messages:
            failures.append(f"{account} did not return from recursive trigger")
    if len(responses) != 2:
        failures.append("the second client did not receive a game response")
    errors = RECURSION_ERROR.findall(log_contents)
    if len(errors) != 4:
        failures.append(f"expected four recursion diagnostics, found {len(errors)}")
    if "chain=" not in log_contents:
        failures.append("recursion diagnostic did not include a chain location")

    total = 7
    passed = max(0, total - len(failures))
    if failures:
        print(f"recursion-depth probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-100:]), file=sys.stderr)
        return 1
    print(f"recursion-depth probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
