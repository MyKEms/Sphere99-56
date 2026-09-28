#!/usr/bin/env python3
"""Exercise deferred temporary-account destruction after client callbacks."""

from __future__ import annotations

import argparse
import re
import socket
import struct
import sys
import time
from pathlib import Path

from modes.account_lifetime import (
    ACCOUNT_NAME,
    ACCOUNT_PASSWORD,
    LINGER_ACCOUNT_NAME,
    LINGER_ACCOUNT_PASSWORD,
)
from run_suite import SANITIZER_OUTPUT_RE, shutdown_failures
from test_world_save_roundtrip import run_server


CALLBACK_MARKER = "CAccount logout callback account='AccountLifetimeProbe'"
DESTRUCTION_MARKER = "CAccount destroyed account='AccountLifetimeProbe'"


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


def _start_uid(response: bytes, find_start_packet) -> int | None:
    start = find_start_packet(response)
    if start is None:
        return None
    return struct.unpack_from(">I", start[1], 1)[0]


def _reconnect_same_character(
    host: str,
    port: int,
    game_port: int,
    account: str,
    password: str,
    expected_uid: int,
    *,
    make_char_play,
    game_relogin,
    recv_until_game_start,
    decode_game_response,
    find_start_packet,
    expect_direct: bool = False,
    expect_char_list: bool = False,
):
    """Reconnect and accept either quick entry or the normal char-list path."""

    client, _, response = game_relogin(
        host, port, account, password, game_port=game_port
    )
    if client is None:
        raise RuntimeError(f"{account} reconnect did not reach the game server")
    try:
        start_uid = _start_uid(response, find_start_packet)
        if start_uid is None:
            if expect_direct:
                raise RuntimeError(f"{account} reconnect did not use the lingering character")
            if not response or response[0] != 0xA9:
                raise RuntimeError(f"{account} reconnect returned no game or character list")
            client.sendall(make_char_play(0))
            response = decode_game_response(recv_until_game_start(client, timeout=30.0))
            start_uid = _start_uid(response, find_start_packet)
        elif expect_char_list:
            raise RuntimeError(f"{account} reconnected directly after the linger expired")
        if start_uid != expected_uid:
            raise RuntimeError(
                f"{account} re-entered UID {start_uid!r}; expected {expected_uid:#x}"
            )
    finally:
        client.close()


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
        game_relogin,
        make_char_create,
        make_char_play,
        recv_until_game_start,
    )

    ini_path = fixture / "sphere.ini"
    ini_text = ini_path.read_text(encoding="ascii")
    ini_text = ini_text.replace("DEBUGLEVEL=0", "DEBUGLEVEL=1")
    linger_match = re.search(r"(?im)^\s*CLIENTLINGER\s*=\s*(\d+)\s*$", ini_text)
    if linger_match is None or int(linger_match.group(1)) <= 0:
        raise RuntimeError("account lifetime fixture has no positive CLIENTLINGER setting")
    linger_seconds = int(linger_match.group(1))
    ini_path.write_text(ini_text, encoding="ascii")
    failures: list[str] = []

    def exercise() -> None:
        client: socket.socket | None = None
        first_uid: int | None = None
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
            response = decode_game_response(recv_until_game_start(client, timeout=30.0))
            first_uid = _start_uid(response, find_start_packet)
            if first_uid is None:
                raise RuntimeError("temporary account character did not enter the world")
        finally:
            if client is not None:
                client.close()
        if first_uid is None:
            raise RuntimeError("temporary account did not produce a character UID")
        time.sleep(0.5)
        _reconnect_same_character(
            args.host,
            args.port,
            args.port + 1000,
            ACCOUNT_NAME,
            ACCOUNT_PASSWORD,
            first_uid,
            make_char_play=make_char_play,
            game_relogin=game_relogin,
            recv_until_game_start=recv_until_game_start,
            decode_game_response=decode_game_response,
            find_start_packet=find_start_packet,
            expect_direct=True,
        )

        # A regular account must still support the normal character-list path
        # once CLIENTLINGER expires; this distinguishes quick re-login from a
        # dropped account/character.
        linger_client, _ = game_connect(
            args.host,
            args.port,
            LINGER_ACCOUNT_NAME,
            LINGER_ACCOUNT_PASSWORD,
            game_port=args.port + 1000,
        )
        if linger_client is None:
            raise RuntimeError("linger account did not reach the character list")
        try:
            linger_client.sendall(make_char_create(name=LINGER_ACCOUNT_NAME, start_loc=1))
            linger_response = decode_game_response(
                recv_until_game_start(linger_client, timeout=30.0)
            )
            linger_uid = _start_uid(linger_response, find_start_packet)
            if linger_uid is None:
                raise RuntimeError("linger account character did not enter the world")
        finally:
            linger_client.close()
        time.sleep(linger_seconds + 2.0)
        _reconnect_same_character(
            args.host,
            args.port,
            args.port + 1000,
            LINGER_ACCOUNT_NAME,
            LINGER_ACCOUNT_PASSWORD,
            linger_uid,
            make_char_play=make_char_play,
            game_relogin=game_relogin,
            recv_until_game_start=recv_until_game_start,
            decode_game_response=decode_game_response,
            find_start_packet=find_start_packet,
            expect_char_list=True,
        )

        # The callback is run by CClient::DeleteThis; destruction must wait
        # until the client callback/socket flush and the lingering character
        # have completed.  The second temporary-account logout above starts
        # that final destruction window.
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
    if callback_count != 2:
        failures.append(
            f"temporary account logout callback diagnostic occurred {callback_count} times; expected twice"
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
