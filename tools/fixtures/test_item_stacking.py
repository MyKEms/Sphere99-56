#!/usr/bin/env python3
"""Verify explicit and no-point equal-item merges retain their pile locations."""

from __future__ import annotations

import argparse
import os
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, wait_for_port


ACCOUNT = "StackingProbe"
PASSWORD = "stacking_pw"
STACK_ITEM_ID = 0x0E96
STACKING_UIDS = {0x40000064, 0x40000065}
NO_POINT_STACK_ITEM_ID = 0x0E97


def packets(data: bytes):
    from uo_packets import split_packet_stream

    return split_packet_stream(data, allow_truncated=True)


def decode(data: bytes) -> bytes:
    from uo_test_client import decode_game_response

    return decode_game_response(data)


def drain(sock: socket.socket, timeout: float = 0.8) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.setblocking(False)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except BlockingIOError:
                time.sleep(0.02)
                continue
            if not chunk:
                break
            data.extend(chunk)
    finally:
        sock.setblocking(True)
        sock.settimeout(10.0)
    return bytes(data)


def item_packets(data: bytes) -> list[dict[str, int]]:
    result = []
    for packet in packets(decode(data)):
        if packet.command != 0x1A or len(packet.data) < 14:
            continue
        raw = packet.data
        wire_uid = struct.unpack_from(">I", raw, 3)[0]
        cursor = 9
        amount = 1
        if wire_uid & 0x80000000:
            if len(raw) < cursor + 2:
                continue
            amount = struct.unpack_from(">H", raw, cursor)[0]
            cursor += 2
        if len(raw) < cursor + 5:
            continue
        result.append(
            {
                "uid": wire_uid & 0x7FFFFFFF,
                "id": struct.unpack_from(">H", raw, 7)[0],
                "amount": amount,
            }
        )
    return result


def container_adds(data: bytes) -> list[dict[str, int]]:
    result = []
    for packet in packets(decode(data)):
        if packet.command != 0x25 or len(packet.data) < 18:
            continue
        raw = packet.data
        result.append(
            {
                "uid": struct.unpack_from(">I", raw, 1)[0],
                "id": struct.unpack_from(">H", raw, 5)[0],
                "amount": struct.unpack_from(">H", raw, 8)[0],
                "x": struct.unpack_from(">H", raw, 10)[0],
                "y": struct.unpack_from(">H", raw, 12)[0],
                "container": struct.unpack_from(">I", raw, 14)[0],
            }
        )
    return result


def backpack_uid(data: bytes) -> int | None:
    for packet in packets(decode(data)):
        if packet.command != 0x2E or len(packet.data) < 13:
            continue
        raw = packet.data
        if struct.unpack_from(">H", raw, 5)[0] == 0x0E75 and raw[8] == 0x15:
            return struct.unpack_from(">I", raw, 1)[0]
    # Character entry sends the attached equipment in a 0x78 view packet when
    # the normal 0x2e equipped-item packet is omitted.
    for packet in packets(decode(data)):
        if packet.command != 0x78 or len(packet.data) < 26:
            continue
        raw = packet.data
        offset = 19
        while offset + 7 <= len(raw):
            uid = struct.unpack_from(">I", raw, offset)[0]
            if uid == 0:
                break
            item_id = struct.unpack_from(">H", raw, offset + 4)[0]
            layer = raw[offset + 6]
            offset += 9 if item_id & 0x8000 else 7
            if (item_id & 0x7FFF) == 0x0E75 and layer == 0x15:
                return uid
    return None


def move_to_pack(
    sock: socket.socket,
    uid: int,
    pack_uid: int,
    *,
    x: int = 50,
    y: int = 50,
) -> bytes:
    sock.sendall(struct.pack(">BIH", 0x07, uid, 0))
    response = drain(sock, 0.25)
    sock.sendall(struct.pack(">BIHHBI", 0x08, uid, x, y, 0, pack_uid))
    return response + drain(sock, 0.9)


def run_probe(fixture: Path, binary: Path, port: int, startup_timeout: float) -> int:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_relogin,
        make_char_play,
        recv_until_game_start,
    )

    log_path = fixture / "server.log"
    failures: list[str] = []
    data = bytearray()
    process: subprocess.Popen[bytes] | None = None
    sock: socket.socket | None = None
    returncode: int | None = None
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            env=os.environ.copy(),
        )
        try:
            wait_for_port("127.0.0.1", port, startup_timeout)
            sock, _auth, initial = game_relogin(
                "127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000
            )
            if sock is None:
                failures.append("stacking probe did not reach the character list")
            else:
                sock.sendall(make_char_play(0))
                entry = recv_until_game_start(sock, timeout=10.0)
                data.extend(entry)
                data.extend(drain(sock, 1.0))
                if not entry or find_start_packet(decode_game_response(entry)) is None:
                    failures.append("stacking probe character did not enter the world")
                else:
                    ground = [
                        item
                        for item in item_packets(bytes(data))
                        if item["id"] == STACK_ITEM_ID
                        and item["uid"] in STACKING_UIDS
                    ]
                    pack_uid = backpack_uid(bytes(data))
                    if len({item["uid"] for item in ground}) != 2:
                        failures.append(f"expected two ground piles, got {ground!r}")
                    if pack_uid is None:
                        failures.append("character entry did not expose its backpack")
                    else:
                        by_uid = {item["uid"] for item in ground}
                        for uid in sorted(by_uid):
                            data.extend(move_to_pack(sock, uid, pack_uid))
                        # Re-open the pack so the assertion observes the
                        # retained scripted no-point pile even when the stack
                        # operation itself does not emit a second packet.
                        sock.sendall(struct.pack(">BI", 0x06, pack_uid))
                        data.extend(drain(sock, 0.9))
                        observed = [
                            item
                            for item in container_adds(bytes(data))
                            if item["id"] == STACK_ITEM_ID
                        ]
                        if not any(item["amount"] >= 2 for item in observed):
                            failures.append(
                                "same-definition piles did not merge at explicit point: "
                                f"container_adds={observed!r}, item_packets={item_packets(bytes(data))!r}"
                            )
                        no_point_observed = [
                            item
                            for item in container_adds(bytes(data))
                            if item["id"] == NO_POINT_STACK_ITEM_ID
                        ]
                        if not any(
                            item["amount"] >= 2
                            and item["x"] == 70
                            and item["y"] == 70
                            for item in no_point_observed
                        ):
                            failures.append(
                                "no-point same-definition pile did not retain the "
                                "existing contained point: "
                                f"container_adds={no_point_observed!r}"
                            )
        except (OSError, RuntimeError, struct.error, ValueError) as error:
            failures.append(f"protocol probe raised {type(error).__name__}: {error}")
        finally:
            if sock is not None:
                sock.close()
            returncode = stop_server(process)

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print("item-stacking probe failed: " + "; ".join(failures), file=sys.stderr)
        print(log_contents[-4000:], file=sys.stderr)
        return 1
    print(
        "item-stacking probe passed: explicit and no-point equal-definition "
        "merges retained their contained points"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2852)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()
    return run_probe(args.fixture.resolve(), args.binary.resolve(), args.port, args.startup_timeout)


if __name__ == "__main__":
    raise SystemExit(main())
