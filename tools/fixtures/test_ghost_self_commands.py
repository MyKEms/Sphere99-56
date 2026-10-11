#!/usr/bin/env python3
"""Verify ordinary-player self commands remain usable while dead."""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from make_ghost_self_commands_fixture import (
    GM_ACCOUNT,
    GM_PASSWORD,
    PLAYER_ACCOUNT,
    PLAYER_PASSWORD,
    PLAYER_SERIAL,
)
from run_suite import shutdown_failures


def _talk(text: str) -> bytes:
    payload = text.encode("ascii") + b"\0"
    return struct.pack(">BHBHH", 0x03, 8 + len(payload), 0, 0, 3) + payload


def _target(context: int, uid: int) -> bytes:
    packet = bytearray(19)
    packet[0] = 0x6C
    packet[2:6] = context.to_bytes(4, "big")
    packet[7:11] = uid.to_bytes(4, "big")
    return bytes(packet)


def _packets(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _messages(data: bytes) -> list[str]:
    return [
        packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        for packet in _packets(data)
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def _drain(sock: socket.socket, timeout: float = 1.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.1)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def _recv_until(sock: socket.socket, predicate, timeout: float = 5.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        if predicate(bytes(data)):
            return bytes(data)
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
    return bytes(data)


def _target_context(data: bytes) -> int | None:
    for packet in _packets(data):
        if packet.command == 0x6C and len(packet.data) >= 6:
            context = int.from_bytes(packet.data[2:6], "big")
            if context:
                return context
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3170)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    failures: list[str] = []
    player_sock: socket.socket | None = None
    gm_sock: socket.socket | None = None
    observations: dict[str, list[str] | bool] = {}

    def exercise() -> None:
        nonlocal player_sock, gm_sock
        gm_sock, _ = game_connect(
            args.host, args.port, GM_ACCOUNT, GM_PASSWORD, game_port=args.port + 1000
        )
        player_sock, _ = game_connect(
            args.host,
            args.port,
            PLAYER_ACCOUNT,
            PLAYER_PASSWORD,
            game_port=args.port + 1000,
        )
        if gm_sock is None or player_sock is None:
            raise RuntimeError("ghost self-command fixture did not reach the character list")
        gm_sock.sendall(make_char_play(0))
        if not recv_until_game_start(gm_sock, timeout=30.0):
            raise RuntimeError("GM character did not enter the world")
        player_sock.sendall(make_char_play(0))
        if not recv_until_game_start(player_sock, timeout=30.0):
            raise RuntimeError("player character did not enter the world")
        _drain(gm_sock)
        _drain(player_sock)

        gm_sock.sendall(_talk(".KILL"))
        target_data = _recv_until(gm_sock, lambda value: _target_context(value) is not None)
        context = _target_context(target_data)
        if context is None:
            raise RuntimeError("GM KILL did not open a target cursor")
        gm_sock.sendall(_target(context, PLAYER_SERIAL))
        death_data = _recv_until(
            player_sock,
            lambda value: any(packet.command == 0xAF for packet in _packets(value)),
            timeout=3.0,
        )
        observations["death"] = bool(death_data)

        for command in (".where", ".whoami", ".explevel"):
            player_sock.sendall(_talk(command))
            if command == ".explevel":
                response = _recv_until(
                    player_sock,
                    lambda value: any(packet.command in (0xB0, 0xDD) for packet in _packets(value)),
                    timeout=5.0,
                )
                observations[command] = any(
                    packet.command in (0xB0, 0xDD) for packet in _packets(response)
                )
            else:
                response = _recv_until(
                    player_sock,
                    lambda value: bool(_messages(value)),
                    timeout=5.0,
                )
                observations[command] = _messages(response)
    def close_clients() -> None:
        if player_sock is not None:
            player_sock.close()
        if gm_sock is not None:
            gm_sock.close()

    try:
        returncode, runner_error, log_contents = run_server(
            fixture=fixture,
            binary=binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=fixture / "server.log",
            action=exercise,
        )
    finally:
        close_clients()

    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if observations.get("death") is not True:
        failures.append("player death was not observed")
    for command in (".where", ".whoami"):
        messages = observations.get(command)
        if not isinstance(messages, list) or not messages:
            failures.append(f"{command} produced no system message while dead")
        elif any(message.startswith("Can't '") for message in messages):
            failures.append(f"{command} was rejected while dead: {messages!r}")
        elif command == ".where" and not any("(128,128,0)" in message for message in messages):
            failures.append(f"{command} did not report the player's position: {messages!r}")
        elif command == ".whoami" and "GHOST_SELF_WHOAMI_OK" not in messages:
            failures.append(f"{command} did not execute its self-command handler: {messages!r}")
    if observations.get(".explevel") is not True:
        failures.append(".explevel did not open a gump while dead")

    if failures:
        print(f"observations: {observations!r}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("ghost self-command probe passed: 4/4 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
