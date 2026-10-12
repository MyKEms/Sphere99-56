#!/usr/bin/env python3
"""Check that dense world-entry traffic does not hide the player's backpack."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from run_suite import shutdown_failures, stop_server, wait_for_port
from test_suite import (
    decode_game_response,
    game_connect,
    make_char_create,
    recv_until_game_start,
)
from uo_packets import split_packet_stream


ACCOUNT = "SpawnEntryProbe"
PASSWORD = "spawnpw"
BACKPACK_ID = 0x0E75
BACKPACK_LAYER = 0x15


def _has_backpack(data: bytes) -> bool:
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command == 0x2E and len(packet.data) >= 9:
            item_id = struct.unpack_from(">H", packet.data, 5)[0]
            if item_id == BACKPACK_ID and packet.data[8] == BACKPACK_LAYER:
                return True
        if packet.command != 0x78 or len(packet.data) < 26:
            continue
        offset = 19
        while offset + 7 <= len(packet.data):
            serial = struct.unpack_from(">I", packet.data, offset)[0]
            if serial == 0:
                break
            item_id = struct.unpack_from(">H", packet.data, offset + 4)[0] & 0x7FFF
            layer = packet.data[offset + 6]
            raw_item_id = struct.unpack_from(">H", packet.data, offset + 4)[0]
            offset += 9 if raw_item_id & 0x8000 else 7
            if item_id == BACKPACK_ID and layer == BACKPACK_LAYER:
                return True
    return False


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    log_path = fixture / "server.log"
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, 120.0)
            sock, _ = game_connect(
                "127.0.0.1", args.port, ACCOUNT, PASSWORD, game_port=args.port
            )
            if sock is None:
                failures.append("probe account did not reach the character list")
            else:
                try:
                    sock.sendall(make_char_create(name="DenseEntry", sex=0, start_loc=0))
                    response = decode_game_response(recv_until_game_start(sock, timeout=15.0))
                    if not _has_backpack(response):
                        failures.append(
                            "player entry did not expose its backpack before the dense world view"
                        )
                finally:
                    sock.close()
    except (OSError, RuntimeError, socket.timeout) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    log_contents = (
        log_path.read_text(encoding="utf-8", errors="replace")
        if log_path.exists()
        else ""
    )
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print("spawn entry burst fixture failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("spawn entry burst fixture passed: backpack preceded the dense world view")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
