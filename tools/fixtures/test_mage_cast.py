#!/usr/bin/env python3
"""Verify Magery 30 spell casts and the light-state inputs to spell scripts."""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from modes.mage_cast import (
    ACCOUNT,
    CAST_MARKER,
    CHAR_SERIAL,
    FAIL_MARKER,
    PASSWORD,
    READY_MARKER,
    SUCCESS_MARKER,
)
from run_suite import shutdown_failures


def _messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(
        decode_game_response(data), allow_truncated=True
    ):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def _cast(spell: int) -> bytes:
    name = f"{spell}\0".encode("ascii")
    return struct.pack(">BHB", 0x12, 4 + len(name), 39) + name


def _target(context: int, serial: int) -> bytes:
    return struct.pack(">BBIBIHHBBH", 0x6C, 1, context, 0, serial, 128, 128, 0, 0, 0)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3170)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--casts", type=int, default=20)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("mage-cast probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            initial = recv_until_game_start(sock, timeout=30.0)
            if not initial:
                raise RuntimeError("mage-cast probe character did not enter the world")
            data = bytearray(initial)
            sock.settimeout(0.2)

            def pump(seconds: float) -> None:
                deadline = time.monotonic() + seconds
                while time.monotonic() < deadline:
                    try:
                        chunk = sock.recv(65536)
                    except socket.timeout:
                        continue
                    if not chunk:
                        return
                    data.extend(chunk)

            pump(0.5)
            for spell in (6, 10):
                for _ in range(args.casts):
                    before_cast = len(data)
                    sock.sendall(_cast(spell))
                    context: int | None = None
                    deadline = time.monotonic() + 5.0
                    while time.monotonic() < deadline:
                        current = _messages(bytes(data[before_cast:]))
                        if context is None:
                            from uo_packets import split_packet_stream
                            from uo_test_client import decode_game_response

                            for packet in split_packet_stream(
                                decode_game_response(bytes(data[before_cast:])),
                                allow_truncated=True,
                            ):
                                if packet.command == 0x6C and len(packet.data) >= 6:
                                    context = struct.unpack_from(">I", packet.data, 2)[0]
                                    break
                            if context is not None:
                                sock.sendall(_target(context, CHAR_SERIAL))
                        if (
                            any(message.startswith(SUCCESS_MARKER) for message in current)
                            or any(message.startswith(FAIL_MARKER) for message in current)
                            or any("Kouzlo se nezdarilo" in message for message in current)
                        ):
                            break
                        try:
                            chunk = sock.recv(65536)
                        except socket.timeout:
                            continue
                        if not chunk:
                            return
                        data.extend(chunk)
                    else:
                        raise RuntimeError(f"spell {spell} cast did not complete")
            messages[:] = _messages(bytes(data))
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

    ready = [message for message in messages if message.startswith(READY_MARKER)]
    casts = [message for message in messages if message.startswith(CAST_MARKER)]
    successes = [message for message in messages if message.startswith(SUCCESS_MARKER)]
    fizzles = [message for message in messages if message.startswith(FAIL_MARKER)]
    if not ready or ready[0] != (
        f"{READY_MARKER}|[flag_nightsight=0|magery=30.0|eval_magery=300|"
        "sector_light=17|near_light=0]"
    ):
        failures.append(f"unexpected mage state: {ready!r}")
    if len(casts) != args.casts * 2:
        failures.append(f"spell-cast marker count {len(casts)} != {args.casts * 2}")
    for spell in (6, 10):
        spell_token = f"{spell:02x}"
        cast_token = str(spell)
        if not any(
            message.startswith(f"{CAST_MARKER}|[flag_nightsight=")
            and f"|spell={cast_token}|difficulty=" in message
            and message.endswith("|sector_light=17|near_light=0]")
            for message in casts
        ):
            failures.append(f"spell {spell} did not record the light-state fields")
        spell_successes = [
            message
            for message in successes
            if message == f"{SUCCESS_MARKER}|[{spell_token}]"
        ]
        if len(spell_successes) < args.casts // 2:
            failures.append(
                f"spell {spell} successes {len(spell_successes)} < {args.casts // 2}; "
                f"fizzles={len(fizzles)}"
            )
    if any("Je spatne videt." in message for message in messages):
        failures.append("a no-light fizzle was reported")
    if failures:
        print("mage-cast probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print(
        "mage-cast probe passed: "
        f"{len(successes)} successes, {len(fizzles)} fizzles, "
        f"{len(casts)} spell-state rows"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
