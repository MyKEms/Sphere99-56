#!/usr/bin/env python3
"""Verify that an NPC on logical plane 35 receives normal sector ticks."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path

from modes.npc_plane_wake import ACCOUNT, MARKER, PASSWORD
from run_suite import shutdown_failures, stop_server, wait_for_port


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _system_messages(packets) -> list[str]:
    return [
        packet.data[44:].split(b"\0", 1)[0].decode("latin1", errors="replace")
        for packet in packets
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=4640)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--seconds", type=float, default=15.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    failures: list[str] = []
    packets = []
    process = None
    returncode = None
    try:
        with (fixture / "server.log").open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, args.startup_timeout)
            sock, _ = game_connect(
                "127.0.0.1", args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
            )
            if sock is None:
                raise RuntimeError("NPC plane wake fixture did not reach the character list")
            try:
                sock.sendall(make_char_play(0))
                initial = recv_until_game_start(sock, timeout=30.0)
                if not initial:
                    raise RuntimeError("NPC plane wake fixture character did not enter the world")
                packets.extend(_decode(initial))
                # Discard login animations and collect only the steady-state stream.
                sock.settimeout(0.2)
                deadline = time.monotonic() + args.seconds
                while time.monotonic() < deadline:
                    try:
                        chunk = sock.recv(65536)
                    except socket.timeout:
                        continue
                    if not chunk:
                        break
                    packets.extend(_decode(chunk))
            finally:
                sock.close()
    except (OSError, RuntimeError, ValueError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = (fixture / "server.log").read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, log_contents))
    messages = _system_messages(packets)
    wake_rows = [message for message in messages if message.startswith(MARKER + "|")]
    hits = [int(message.split("|", 1)[1]) for message in wake_rows if "|" in message]
    if len(wake_rows) < 5:
        failures.append(f"plane wake driver produced only {len(wake_rows)} timer rows")
    if len(hits) < 2 or all(value == hits[0] for value in hits[1:]):
        failures.append(f"plane-35 NPC did not receive a normal tick: hits={hits!r}")

    if failures:
        print("NPC plane wake probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "NPC plane wake probe passed: "
        f"{len(wake_rows)} timer rows with hit-state changes {hits[0]} -> {hits[-1]}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
