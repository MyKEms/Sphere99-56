#!/usr/bin/env python3
"""Check exact privilege-table lookup and obscene-name rejection."""

from __future__ import annotations

import argparse
import re
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from modes.priv_commands import (
    ADMIN_COMMAND,
    CREATE_ACCOUNT,
    CREATE_PASSWORD,
    GM_ACCOUNT,
    GM_PASSWORD,
    OBSCENE_NAME,
    PLAYER_ACCOUNT,
    PLAYER_COMMAND,
    PLAYER_PASSWORD,
    UNLISTED_COMMAND,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


COMMAND_RE = re.compile(
    r"commands uid=0x?[0-9a-fA-F]+.* to '([^']+)' (OK|NO PRIV)",
    re.IGNORECASE,
)


def _talk(text: str) -> bytes:
    encoded = text.encode("ascii") + b"\0"
    return struct.pack(">BHBHH", 0x03, 8 + len(encoded), 0, 0, 3) + encoded


def _read_logs(fixture: Path) -> str:
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


def _enter_and_send(host: str, port: int, account: str, password: str, commands: tuple[str, ...]) -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"{account} did not reach its character list")
    try:
        sock.sendall(make_char_play(0))
        if not recv_until_game_start(sock, timeout=30.0):
            raise RuntimeError(f"{account} did not enter the world")
        for command in commands:
            sock.sendall(_talk(f".{command}"))
            time.sleep(0.25)
    finally:
        sock.close()


def _try_obscene_create(host: str, port: int) -> None:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import game_connect, make_char_create

    sock, _ = game_connect(host, port, CREATE_ACCOUNT, CREATE_PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("obscene-name account did not reach its character list")
    try:
        sock.sendall(make_char_create(name=OBSCENE_NAME, start_loc=1))
        sock.settimeout(0.2)
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            try:
                if not sock.recv(65536):
                    break
            except socket.timeout:
                continue
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2960)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    with (fixture / "server.log").open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            _enter_and_send(
                args.host,
                args.port,
                PLAYER_ACCOUNT,
                PLAYER_PASSWORD,
                (PLAYER_COMMAND.lower(), ADMIN_COMMAND, UNLISTED_COMMAND),
            )
            _enter_and_send(
                args.host,
                args.port,
                GM_ACCOUNT,
                GM_PASSWORD,
                (ADMIN_COMMAND, UNLISTED_COMMAND),
            )
            _try_obscene_create(args.host, args.port)
        except (OSError, RuntimeError, socket.timeout) as error:
            failures.append(str(error))
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    contents = _read_logs(fixture)
    failures.extend(shutdown_failures(returncode, contents))
    records = [(command.upper(), status.upper()) for command, status in COMMAND_RE.findall(contents)]
    expected = (
        (PLAYER_COMMAND, "OK"),
        (ADMIN_COMMAND, "NO PRIV"),
        (UNLISTED_COMMAND, "NO PRIV"),
        (ADMIN_COMMAND, "NO PRIV"),
        (UNLISTED_COMMAND, "OK"),
    )
    for command, status in expected:
        if (command, status) not in records:
            failures.append(f"missing privilege result {command} {status}; records={records!r}")
    if f"Obscene name '{OBSCENE_NAME}' ignored." not in contents:
        failures.append("obscene-name creation was not rejected by the resource list")

    if failures:
        print("privilege command probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- logs ---", file=sys.stderr)
        print("\n".join(contents.splitlines()[-160:]), file=sys.stderr)
        return 1
    print("privilege command probe passed: 5 command results and obscene-name rejection")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
