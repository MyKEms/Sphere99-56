#!/usr/bin/env python3
"""Verify that [PLEVEL n] command lists grant and restrict spoken commands."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

from modes.compat_writer import (
    GM_COMMAND_LOG_ACCOUNT,
    GM_COMMAND_LOG_PASSWORD,
    GM_COMMAND_LOG_PLAYER_ACCOUNT,
    GM_COMMAND_LOG_PLAYER_PASSWORD,
)
from modes.plevel_commands import MARKER, OWNER_COMMAND, PLAYER_COMMAND, UNLISTED_COMMAND


COMMAND_LOG = re.compile(
    r"'([^']+)' commands uid=0x?[0-9a-fA-F]+.* to '([^']+)' (OK|NO PRIV)",
    re.IGNORECASE,
)

# (account, spoken command, expected log result).  The admin account sits
# below the owner level and above the GM default.
EXPECTED = (
    (GM_COMMAND_LOG_PLAYER_ACCOUNT, PLAYER_COMMAND, "OK"),
    (GM_COMMAND_LOG_PLAYER_ACCOUNT, PLAYER_COMMAND.upper(), "OK"),
    (GM_COMMAND_LOG_PLAYER_ACCOUNT, f"{PLAYER_COMMAND} 1", "OK"),
    (GM_COMMAND_LOG_PLAYER_ACCOUNT, UNLISTED_COMMAND, "NO PRIV"),
    (GM_COMMAND_LOG_PLAYER_ACCOUNT, OWNER_COMMAND, "NO PRIV"),
    (GM_COMMAND_LOG_ACCOUNT, PLAYER_COMMAND, "OK"),
    (GM_COMMAND_LOG_ACCOUNT, UNLISTED_COMMAND, "OK"),
    (GM_COMMAND_LOG_ACCOUNT, OWNER_COMMAND, "NO PRIV"),
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


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def speak_commands(host: str, port: int, account: str, password: str, commands: list[str]) -> list[str]:
    """Speak each command as a player would and return the system messages seen."""
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import game_connect, make_char_play, make_unicode_talk, recv_until_game_start

    sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"PLEVEL probe did not reach the character list for {account}")
    try:
        sock.sendall(make_char_play(0))
        data = bytearray(recv_until_game_start(sock, timeout=30.0) or b"")
        time.sleep(0.5)
        for command in commands:
            sock.sendall(make_unicode_talk("." + command, mode=0))
            time.sleep(0.3)
        deadline = time.monotonic() + 2.0
        sock.settimeout(0.2)
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            data.extend(chunk)
        return system_messages(bytes(data))
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2994)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from run_suite import shutdown_failures, stop_server, wait_for_port

    fixture = args.fixture.resolve()
    failures: list[str] = []
    seen_messages: dict[str, list[str]] = {}
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
            for account, password in (
                (GM_COMMAND_LOG_PLAYER_ACCOUNT, GM_COMMAND_LOG_PLAYER_PASSWORD),
                (GM_COMMAND_LOG_ACCOUNT, GM_COMMAND_LOG_PASSWORD),
            ):
                commands = [command for owner, command, _ in EXPECTED if owner == account]
                seen_messages[account] = speak_commands(
                    args.host, args.port, account, password, commands
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
    records = [
        (account.casefold(), command.casefold(), status.upper())
        for account, command, status in COMMAND_LOG.findall(contents)
    ]
    for account, command, status in EXPECTED:
        key = (account.casefold(), command.casefold())
        seen = [result for name, spoken, result in records if (name, spoken) == key]
        if status not in seen:
            failures.append(f"{account} '.{command}': expected {status}, logged {seen or 'nothing'}")
    player_messages = seen_messages.get(GM_COMMAND_LOG_PLAYER_ACCOUNT, [])
    if not any(f"{MARKER} player command ran" in text for text in player_messages):
        failures.append("the player-listed command did not run")
    for refused in ("owner", "unlisted"):
        if any(f"{MARKER} {refused} command ran" in text for text in player_messages):
            failures.append(f"the player ran the {refused} command")

    if failures:
        print("PLEVEL command probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- logs ---", file=sys.stderr)
        print("\n".join(contents.splitlines()[-80:]), file=sys.stderr)
        return 1
    print(f"PLEVEL command probe passed: {len(EXPECTED)}/{len(EXPECTED)} privilege checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
