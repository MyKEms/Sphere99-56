#!/usr/bin/env python3
"""Check that NEWLOOT dispatches a template through the character receiver."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures

from modes.newloot import ACCOUNT, MARKER, PASSWORD


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def _parse(messages: list[str], label: str) -> tuple[int, ...] | None:
    prefix = f"{MARKER}_{label} "
    for message in messages:
        if not message.startswith(prefix):
            continue
        values = message[len(prefix) :].split("|")
        try:
            parsed: list[int] = []
            for value in values:
                token = value.strip().lstrip("#")
                try:
                    parsed.append(int(token, 0))
                except ValueError:
                    # Sphere serials are emitted as zero-prefixed hexadecimal
                    # values without a 0x marker; amounts remain decimal.
                    parsed.append(int(token, 16))
            return tuple(parsed)
        except ValueError:
            return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3130)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("NEWLOOT probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("NEWLOOT probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if f"{MARKER}_END" in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            messages[:] = system_messages(bytes(data))
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if messages.count(f"{MARKER}_END") != 1:
        failures.append(f"end marker count {messages.count(f'{MARKER}_END')} != 1")

    equipped = _parse(messages, "EQUIP")
    if equipped is None or len(equipped) != 2 or not all(equipped):
        failures.append(f"equipped layer UIDs are missing: {equipped!r}")
    pack = _parse(messages, "PACK")
    if (
        pack is None
        or len(pack) != 4
        or not pack[0]
        or not pack[1]
        or pack[2] != 3
        or pack[3] != 0x456
    ):
        failures.append(f"template pack item is wrong: {pack!r}")
    lastnew = _parse(messages, "LASTNEW")
    if lastnew is None or len(lastnew) != 2 or not lastnew[0] or lastnew[1] != 3:
        failures.append(f"LASTNEW does not identify the final template item: {lastnew!r}")
    if pack and lastnew and pack[0] != lastnew[0]:
        failures.append(f"LASTNEW serial {lastnew[0]} differs from pack serial {pack[0]}")
    if failures:
        print("NEWLOOT probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("NEWLOOT probe passed: template equipped layerable items and packed the rest")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
