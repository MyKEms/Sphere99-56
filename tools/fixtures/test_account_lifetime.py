#!/usr/bin/env python3
"""Exercise deferred temporary-account destruction after client callbacks."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.account_lifetime import ACCOUNT_NAME, ACCOUNT_PASSWORD
from run_suite import SANITIZER_OUTPUT_RE, shutdown_failures
from test_world_save_roundtrip import run_server


CALLBACK_MARKER = "CAccount logout callback"
DESTRUCTION_MARKER = "CAccount destroyed"


def _wait_for_markers(log_path: Path, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            contents = log_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            contents = ""
        if CALLBACK_MARKER in contents and DESTRUCTION_MARKER in contents:
            return
        time.sleep(0.1)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2866)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    ini_path = fixture / "sphere.ini"
    ini_path.write_text(
        ini_path.read_text(encoding="ascii").replace("DEBUGLEVEL=0", "DEBUGLEVEL=1"),
        encoding="ascii",
    )
    failures: list[str] = []

    def exercise() -> None:
        client: socket.socket | None = None
        try:
            client, _ = game_connect(
                args.host,
                args.port,
                ACCOUNT_NAME,
                ACCOUNT_PASSWORD,
                game_port=args.port + 1000,
            )
            if client is None:
                raise RuntimeError("temporary account did not reach the character list")
            client.sendall(make_char_create(name=ACCOUNT_NAME, start_loc=1))
            response = recv_until_game_start(client, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("temporary account character did not enter the world")
        finally:
            if client is not None:
                client.close()
        # The callback is run by CClient::DeleteThis; destruction must wait
        # until the client dispatch and socket flush have completed.
        _wait_for_markers(fixture / "server.log", timeout=15.0)

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
    if SANITIZER_OUTPUT_RE.search(log_contents):
        failures.append("server log contains sanitizer output")

    callback_count = log_contents.count(CALLBACK_MARKER)
    destruction_count = log_contents.count(DESTRUCTION_MARKER)
    if callback_count != 1:
        failures.append(
            f"account logout callback diagnostic occurred {callback_count} times; expected exactly once"
        )
    if destruction_count != 1:
        failures.append(
            f"account destruction diagnostic occurred {destruction_count} times; expected exactly once"
        )
    callback_offset = log_contents.find(CALLBACK_MARKER)
    destruction_offset = log_contents.find(DESTRUCTION_MARKER)
    if (
        callback_offset >= 0
        and destruction_offset >= 0
        and destruction_offset < callback_offset
    ):
        failures.append("account was destroyed before the logout callback completed")

    if failures:
        print("account lifetime probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1

    print("account lifetime probe passed: callback completed before account destruction")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
