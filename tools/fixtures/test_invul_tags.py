#!/usr/bin/env python3
"""Check player and privileged views of an invulnerable NPC's name."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.invul_tags import (
    NPC_SERIAL,
    PLAYER_ACCOUNT,
    PLAYER_PASSWORD,
    STAFF_ACCOUNT,
    STAFF_PASSWORD,
)
from run_suite import shutdown_failures

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


def _packets(raw: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(raw), allow_truncated=True)


def _label(packet) -> str:
    if packet.command != 0x1C or len(packet.data) < 45:
        return ""
    return packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")


def _speech_uid(packet) -> int:
    if packet.command != 0x1C or len(packet.data) < 7:
        return 0
    return int.from_bytes(packet.data[3:7], "big")


def _enter(host: str, port: int, account: str, password: str) -> socket.socket:
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"{account} did not reach the character list")
    sock.sendall(make_char_play(0))
    if not recv_until_game_start(sock, timeout=30.0):
        sock.close()
        raise RuntimeError(f"{account} did not enter the world")
    return sock


def _click_label(sock: socket.socket, uid: int) -> str:
    raw = bytearray()
    deadline = time.monotonic() + 10.0
    sock.settimeout(0.2)
    sock.sendall(bytes([0x09]) + uid.to_bytes(4, "big"))
    while time.monotonic() < deadline:
        for packet in _packets(bytes(raw)):
            if _speech_uid(packet) == uid:
                return _label(packet)
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        raw.extend(chunk)
    return ""


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3160)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    args.fixture = args.fixture.resolve()
    args.binary = args.binary.resolve()
    if not args.binary.is_file():
        parser.error(f"server binary does not exist: {args.binary}")

    from test_world_save_roundtrip import run_server

    failures: list[str] = []
    labels: dict[str, str] = {}
    sockets: list[socket.socket] = []

    def exercise() -> None:
        try:
            player = _enter(args.host, args.port, PLAYER_ACCOUNT, PLAYER_PASSWORD)
            sockets.append(player)
            time.sleep(0.4)
            labels["player"] = _click_label(player, NPC_SERIAL)

            staff = _enter(args.host, args.port, STAFF_ACCOUNT, STAFF_PASSWORD)
            sockets.append(staff)
            time.sleep(0.4)
            labels["staff"] = _click_label(staff, NPC_SERIAL)
        finally:
            for sock in sockets:
                sock.close()

    try:
        returncode, runner_error, log_contents = run_server(
            fixture=args.fixture,
            binary=args.binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=args.fixture / "server.log",
            action=exercise,
        )
    except (OSError, RuntimeError, ValueError) as error:
        returncode, runner_error, log_contents = None, str(error), ""
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    player_label = labels.get("player", "")
    staff_label = labels.get("staff", "")
    if not player_label:
        failures.append("ordinary player did not receive an NPC label")
    elif "[invul]" in player_label.lower() or "[npc]" in player_label.lower():
        failures.append(f"ordinary player saw diagnostic tags: {player_label!r}")
    if not staff_label:
        failures.append("ALLSHOW staff did not receive an NPC label")
    elif "[invul]" not in staff_label.lower():
        failures.append(f"ALLSHOW staff label lacked [invul]: {staff_label!r}")

    if failures:
        print("invul-tags probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "invul-tags probe passed: player label is plain and ALLSHOW label retains [invul]"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
