#!/usr/bin/env python3
"""Check that normal visibility and movement stay on one map plane.

The fixture puts a player on plane 10 and gives the same x/y to an item and
NPC on plane 0.  A same-plane item/NPC must arrive, while the cross-plane
objects must not.  A blocking item on plane 0 is placed on the next step; its
presence must not reject the plane-10 walk.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from modes.plane_visibility import (
    ACCOUNT,
    OTHER_PLANE_BLOCKER_SERIAL,
    OTHER_PLANE_ITEM_SERIAL,
    OTHER_PLANE_NPC_SERIAL,
    PASSWORD,
    SAME_PLANE_DESTINATION_ITEM_SERIAL,
    SAME_PLANE_ITEM_SERIAL,
    SAME_PLANE_NPC_SERIAL,
)
from run_suite import shutdown_failures


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _uid(packet) -> int | None:
    if packet.command == 0x1A and len(packet.data) >= 7:
        return struct.unpack_from(">I", packet.data, 3)[0] & 0x7FFFFFFF
    if packet.command == 0x78 and len(packet.data) >= 7:
        return struct.unpack_from(">I", packet.data, 3)[0] & 0x7FFFFFFF
    if packet.command in (0x20, 0x77) and len(packet.data) >= 5:
        return struct.unpack_from(">I", packet.data, 1)[0] & 0x7FFFFFFF
    return None


def _recv_until_reply(sock: socket.socket, timeout: float = 4.0):
    raw = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        raw.extend(chunk)
        packets = _decode(bytes(raw))
        if any(packet.command in (0x21, 0x22) for packet in packets):
            return packets
    return _decode(bytes(raw))


def run_probe(host: str, port: int, result: dict) -> None:
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect(host, port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("plane-visibility fixture did not reach the character list")
    try:
        sock.sendall(make_char_play(0))
        initial = recv_until_game_start(sock, timeout=30.0)
        decoded = decode_game_response(initial)
        if find_start_packet(decoded) is None:
            raise RuntimeError("plane-visibility fixture character did not enter the world")
        packets = _decode(initial)
        result["item_uids"] = sorted(
            uid for packet in packets if packet.command == 0x1A
            for uid in (_uid(packet),) if uid is not None
        )
        result["mobile_uids"] = sorted(
            uid for packet in packets if packet.command in (0x20, 0x77, 0x78)
            for uid in (_uid(packet),) if uid is not None
        )

        # East is the destination with the cross-plane blocking item.
        sock.sendall(struct.pack(">BBBI", 0x02, 2, 1, 0))
        replies = _recv_until_reply(sock)
        result["walk_replies"] = [
            (packet.command, packet.data[1])
            for packet in replies
            if packet.command in (0x21, 0x22) and len(packet.data) >= 2
        ]
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2890)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server

    result: dict = {}
    failures: list[str] = []

    def exercise() -> None:
        run_probe(args.host, args.port, result)

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

    items = set(result.get("item_uids", ()))
    mobiles = set(result.get("mobile_uids", ()))
    expected_item_uids = {
        SAME_PLANE_ITEM_SERIAL | 0x40000000,
        SAME_PLANE_DESTINATION_ITEM_SERIAL | 0x40000000,
    }
    forbidden_item_uids = {
        OTHER_PLANE_ITEM_SERIAL | 0x40000000,
        OTHER_PLANE_BLOCKER_SERIAL | 0x40000000,
    }
    expected_mobile_uids = {SAME_PLANE_NPC_SERIAL, 3}
    forbidden_mobile_uids = {OTHER_PLANE_NPC_SERIAL}
    if not expected_item_uids.issubset(items):
        failures.append(f"same-plane item packets missing: got {sorted(items)!r}")
    if forbidden_item_uids & items:
        failures.append(f"cross-plane item packets present: {sorted(forbidden_item_uids & items)!r}")
    if not expected_mobile_uids.issubset(mobiles):
        failures.append(f"same-plane mobile packets missing: got {sorted(mobiles)!r}")
    if forbidden_mobile_uids & mobiles:
        failures.append(f"cross-plane mobile packets present: {sorted(forbidden_mobile_uids & mobiles)!r}")

    replies = result.get("walk_replies", [])
    if (0x22, 1) not in replies:
        failures.append(f"plane-10 walk was not acknowledged: replies={replies!r}")
    if any(command == 0x21 and sequence == 1 for command, sequence in replies):
        failures.append("cross-plane blocker rejected the plane-10 walk")

    if failures:
        print("plane visibility probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1
    print(
        "plane visibility probe passed: same-plane item/NPC packets retained, "
        "cross-plane packets filtered, and plane-10 walk acknowledged"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
