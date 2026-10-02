#!/usr/bin/env python3
"""Verify stock-compatible TRY command dispatch and privilege reporting."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import time
from pathlib import Path

from modes.compat_writer import (
    GM_COMMAND_LOG_ACCOUNT,
    GM_COMMAND_LOG_MARKERS,
    GM_COMMAND_LOG_PASSWORD,
    GM_COMMAND_LOG_PLAYER_ACCOUNT,
    GM_COMMAND_LOG_PLAYER_PASSWORD,
)


COMMAND_LOG = re.compile(
    r"commands uid=0x?[0-9a-fA-F]+.* to 's\(([^']+)\)' (OK|NO PRIV)",
    re.IGNORECASE,
)


def read_logs(fixture: Path) -> str:
    chunks: list[str] = []
    server_log = fixture / "server.log"
    if server_log.exists():
        chunks.append(server_log.read_text(encoding="utf-8", errors="replace"))
    for path in sorted((fixture / "logs").glob("sphere*.log")):
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    return "\n".join(chunks)


def login_and_play(host: str, port: int, account: str, password: str) -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"TRY probe did not reach the character list for {account}")
    try:
        sock.sendall(make_char_play(0))
        recv_until_game_start(sock, timeout=30.0)
        time.sleep(0.5)
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2896)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from run_suite import shutdown_failures, stop_server, wait_for_port

    fixture = args.fixture.resolve()
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    with (fixture / "server.log").open("wb") as log_file:
        process = subprocess.Popen(
            [str(args.binary.resolve()), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            login_and_play(args.host, args.port, GM_COMMAND_LOG_ACCOUNT, GM_COMMAND_LOG_PASSWORD)
            login_and_play(
                args.host,
                args.port,
                GM_COMMAND_LOG_PLAYER_ACCOUNT,
                GM_COMMAND_LOG_PLAYER_PASSWORD,
            )
            time.sleep(1.0)
        except Exception as error:  # noqa: BLE001 - preserve bounded probe details
            failures.append(str(error))
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    contents = read_logs(fixture)
    failures.extend(shutdown_failures(returncode, contents))
    records = COMMAND_LOG.findall(contents)
    ok = [value for value, status in records if status.upper() == "OK"]
    no_priv = [value for value, status in records if status.upper() == "NO PRIV"]
    for marker in GM_COMMAND_LOG_MARKERS:
        if not any(marker in value for value in ok):
            failures.append(f"missing successful TRY marker: {marker}")
        if not any(marker in value for value in no_priv):
            failures.append(f"missing privilege-check TRY marker: {marker}")

    report = fixture / "logs" / "unknown-keywords.json"
    if not report.exists():
        failures.append("unknown-keyword report was not written")
    else:
        try:
            entries = json.loads(report.read_text(encoding="utf-8")).get("entries", [])
        except (OSError, ValueError) as error:
            failures.append(f"unknown-keyword report is invalid: {error}")
            entries = []
        try_entries = [entry for entry in entries if entry.get("keyword", "").upper() == "TRY"]
        if try_entries:
            failures.append(f"TRY was reported as unknown: {try_entries!r}")

    if failures:
        print("TRY probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- logs ---", file=sys.stderr)
        print("\n".join(contents.splitlines()[-120:]), file=sys.stderr)
        return 1
    print(
        f"TRY probe passed: {len(GM_COMMAND_LOG_MARKERS)} OK and "
        f"{len(GM_COMMAND_LOG_MARKERS)} NO PRIV records; method TRY not unknown"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
