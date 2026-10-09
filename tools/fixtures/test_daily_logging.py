#!/usr/bin/env python3
"""Verify stock-compatible daily logging, including player speech."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path

from modes.compat_writer import (
    DAILY_LOG_CREATE_ACCOUNT,
    DAILY_LOG_CREATE_NAME,
    DAILY_LOG_CREATE_PASSWORD,
    ESCAPE_OVERFLOW_ACCOUNT,
    ESCAPE_OVERFLOW_PASSWORD,
)


SCRIPT_ERROR = re.compile(r"[^\s]+\(\d+\): WHILE loop stopped after 32 iterations")
DAILY_MARKERS = (
    SCRIPT_ERROR,
    re.compile(r"Client connected \[Total:\d+\] from '[^']+'\."),
    re.compile(r"Login '[^']+'"),
    re.compile(r"Setup_CreateDialog acct='[^']+', char='[^']+'"),
    re.compile(r"Setup_Start acct='[^']+', char='[^']+'"),
    re.compile(r"Client disconnected \[Total:\d+\]"),
    re.compile(r"Says UNICODE 'ENU' 'unicode fixture speech' mode=3"),
    re.compile(r"Says 'bank' mode=0"),
)

# These are implementation traces used while diagnosing the Linux logging
# path.  They are intentionally stderr-only: the daily file is a compatibility
# surface and must contain the stock connection/login records instead.
NON_STOCK_DAILY_MARKERS = (
    "xFlush ",
    "Setup_CreateDialog:",
    "addPlayerStart:",
    "f:Setup_Start done",
)


def read_daily_logs(fixture: Path) -> str:
    chunks = []
    for path in sorted((fixture / "logs").glob("sphere*.log")):
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    return "\n".join(chunks)


def marker_matches(contents: str) -> list[bool]:
    return [
        marker.search(contents) is not None if hasattr(marker, "search") else marker in contents
        for marker in DAILY_MARKERS
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2805)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from run_suite import wait_for_port
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        make_char_play,
        make_tokenized_unicode_talk,
        make_unicode_talk,
        recv_until_game_start,
    )

    server_log = fixture / "server.log"
    failures: list[str] = []
    response = b""
    process: subprocess.Popen[bytes] | None = None
    try:
        with server_log.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port(args.host, args.port, args.startup_timeout)
            create_sock, _ = game_connect(
                args.host,
                args.port,
                DAILY_LOG_CREATE_ACCOUNT,
                DAILY_LOG_CREATE_PASSWORD,
                game_port=args.port + 1000,
            )
            if create_sock is None:
                raise RuntimeError("daily logging probe did not reach the creation character list")
            try:
                create_sock.sendall(make_char_create(name=DAILY_LOG_CREATE_NAME, start_loc=1))
                create_response = recv_until_game_start(create_sock, timeout=30.0)
                if find_start_packet(decode_game_response(create_response)) is None:
                    raise RuntimeError("daily logging probe did not enter after character creation")
            finally:
                create_sock.close()

            sock, _ = game_connect(
                args.host,
                args.port,
                ESCAPE_OVERFLOW_ACCOUNT,
                ESCAPE_OVERFLOW_PASSWORD,
                game_port=args.port + 1000,
            )
            if sock is None:
                raise RuntimeError("daily logging probe did not reach the character list")
            try:
                sock.sendall(make_char_play(0))
                response = recv_until_game_start(sock, timeout=30.0)
                sock.sendall(make_unicode_talk("unicode fixture speech"))
                sock.sendall(make_tokenized_unicode_talk("bank"))
                # Keep the game connection open until the server has had a
                # tick to consume the second variable-length packet.  A
                # close immediately after send can discard a queued packet
                # before the dispatch loop reaches it.
                speech_deadline = time.monotonic() + 3.0
                while time.monotonic() < speech_deadline:
                    if DAILY_MARKERS[-1].search(read_daily_logs(fixture)):
                        break
                    time.sleep(0.1)
            finally:
                sock.close()

            deadline = time.monotonic() + 15.0
            daily_contents = ""
            while time.monotonic() < deadline:
                daily_contents = read_daily_logs(fixture)
                if all(marker_matches(daily_contents)):
                    break
                time.sleep(0.1)

            if not all(marker_matches(daily_contents)):
                failures.append(
                    "daily file did not contain script error, connection, login, setup, and speech markers"
                )
            leaked = [marker for marker in NON_STOCK_DAILY_MARKERS if marker in daily_contents]
            if leaked:
                failures.append(
                    "daily file contained non-stock trace markers: " + ", ".join(leaked)
                )
            if process.poll() is not None:
                failures.append("server exited before the unclean durability check")
            else:
                process.kill()
                process.wait(timeout=10.0)
    except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
        failures.append(str(error))
        if process is not None and process.poll() is None:
            process.kill()
            process.wait(timeout=10.0)

    daily_contents = read_daily_logs(fixture)
    if not all(marker_matches(daily_contents)):
        failures.append("daily markers were not durable after SIGKILL")
    leaked = [marker for marker in NON_STOCK_DAILY_MARKERS if marker in daily_contents]
    if leaked:
        failures.append("non-stock trace markers survived SIGKILL: " + ", ".join(leaked))
    if find_start_packet(decode_game_response(response)) is None:
        failures.append("existing character did not enter the world")

    if failures:
        print("daily logging probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- daily logs ---", file=sys.stderr)
        print(daily_contents, file=sys.stderr)
        return 1

    print(
        "daily logging probe passed: script error, connection, login, setup, and speech "
        "lines reached the daily file before SIGKILL"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
