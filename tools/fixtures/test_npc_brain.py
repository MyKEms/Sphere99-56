#!/usr/bin/env python3
"""Check that named NPC creation does not hit the NPC setter."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "NpcBrainProbe"
LOGIN_TOKEN = "npc-brain-pw"
MARKER = "SPHERE_NPC_BRAIN"
DEFAULT_MARKER = "SPHERE_NPC_BRAIN default"
ALIAS_MARKER = "SPHERE_NPC_BRAIN alias-created"
PLAYER_MARKER = "SPHERE_NPC_BRAIN player-npc=0"


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4594)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            LOGIN_TOKEN,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("NPC brain probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            data = recv_until_game_start(sock, timeout=30.0)
            if not data:
                raise RuntimeError("NPC brain probe character did not enter the world")
            buffer = bytearray(data)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(buffer))
                if f"{MARKER} created" in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                buffer.extend(chunk)
            messages[:] = system_messages(bytes(buffer))
        finally:
            sock.close()

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
    if f"{MARKER} created" not in messages:
        failures.append("named NPC creation event did not execute")
    if not any(message.startswith(DEFAULT_MARKER) for message in messages):
        failures.append("unresolved NPC brain did not preserve a default brain")
    if ALIAS_MARKER not in messages:
        failures.append("legacy BERSERK brain spelling did not create an NPC")
    if PLAYER_MARKER not in messages:
        failures.append("zero NPC assignment did not preserve the player state")
    if "NPC_SetBrain NULL" in log_contents:
        failures.append("NPC brain creation reached the NULL setter path")

    if failures:
        print(f"messages: {messages!r}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("NPC brain creation probe passed: 5/5 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
