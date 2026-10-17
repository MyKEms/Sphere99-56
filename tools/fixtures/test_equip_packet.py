#!/usr/bin/env python3
"""Check that one equip request emits one equipped-item packet."""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from modes.equip_packet import ACCOUNT, ARCHERY_MARKER, LOGIN_CODE, MARKER
from run_suite import shutdown_failures


def packets(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def system_messages(data: bytes) -> list[str]:
    return [
        packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        for packet in packets(data)
        if packet.command == 0x1C and len(packet.data) >= 45
    ]


def drain(sock: socket.socket, timeout: float = 0.4) -> bytes:
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


def parse_serial(value: str) -> int:
    token = value.strip().lstrip("#")
    if not token:
        return 0
    try:
        return int(token, 0)
    except ValueError:
        return int(token, 16)


def equipped_packets(data: bytes, item_uid: int, char_uid: int) -> list[bytes]:
    result = []
    for packet in packets(data):
        if packet.command != 0x2E or len(packet.data) < 13:
            continue
        raw = packet.data
        if (
            struct.unpack_from(">I", raw, 1)[0] == item_uid
            and struct.unpack_from(">I", raw, 9)[0] == char_uid
        ):
            result.append(raw)
    return result


def container_packets(data: bytes, item_uid: int, container_uid: int) -> list[bytes]:
    result = []
    for packet in packets(data):
        if packet.command != 0x25 or len(packet.data) < 18:
            continue
        raw = packet.data
        if (
            struct.unpack_from(">I", raw, 1)[0] == item_uid
            and struct.unpack_from(">I", raw, 14)[0] == container_uid
        ):
            result.append(raw)
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=45222)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    failures: list[str] = []
    equip_rows: list[bytes] = []
    self_remove_rows: list[bytes] = []
    archery_equips: dict[int, list[bytes]] = {}
    archery_bounces: dict[int, list[bytes]] = {}
    archery_layers: tuple[int, int, int, int, int] | None = None

    def exercise() -> None:
        nonlocal archery_layers
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, LOGIN_CODE, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("equip-packet probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            initial = bytearray(recv_until_game_start(sock, timeout=30.0))
            deadline = time.monotonic() + 15.0
            sock.settimeout(0.2)
            ready = None
            while time.monotonic() < deadline:
                for message in system_messages(bytes(initial)):
                    if message.startswith(MARKER + "_READY|"):
                        ready = message
                        break
                if ready is not None:
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                initial.extend(chunk)
            if ready is None:
                raise RuntimeError(
                    "equip-packet fixture did not reach its ready marker: "
                    f"{system_messages(bytes(initial))!r}"
                )
            fields = ready.split("|")
            if len(fields) < 5:
                raise RuntimeError(f"malformed ready marker: {ready!r}")
            start = find_start_packet(decode_game_response(bytes(initial)))
            if start is None:
                raise RuntimeError("equip-packet probe did not receive a start packet")
            item_uid = parse_serial(fields[1])
            char_uid = struct.unpack_from(">I", start[1], 1)[0]
            self_remove_uid = parse_serial(fields[4])

            # The script probe runs from a one-shot timer, after the character
            # has entered the world.  Keep the post-login bytes so the packet
            # counts describe that script request rather than the login image.
            archery_data = bytearray(initial)
            sock.setblocking(False)
            deadline = time.monotonic() + 15.0
            while time.monotonic() < deadline:
                archery_rows = [
                    message
                    for message in system_messages(bytes(archery_data))
                    if message.startswith(ARCHERY_MARKER + "|")
                ]
                if archery_rows:
                    break
                try:
                    chunk = sock.recv(65536)
                except BlockingIOError:
                    time.sleep(0.02)
                    continue
                if not chunk:
                    break
                archery_data.extend(chunk)
            sock.setblocking(True)
            sock.settimeout(10.0)
            archery_rows = [
                message
                for message in system_messages(bytes(archery_data))
                if message.startswith(ARCHERY_MARKER + "|")
            ]
            if len(archery_rows) != 1:
                raise RuntimeError(
                    "equip-packet fixture did not reach its archery marker: "
                    f"{system_messages(bytes(archery_data))!r}"
                )
            archery_values = archery_rows[0].split("|")
            if len(archery_values) != 6:
                raise RuntimeError(f"malformed archery marker: {archery_rows[0]!r}")
            archery_uids = [parse_serial(value) for value in archery_values[1:4]]
            archery_layers = (
                *archery_uids,
                parse_serial(archery_values[4]),
                parse_serial(archery_values[5]),
            )
            archery_equips.clear()
            archery_equips.update(
                {
                    uid: equipped_packets(bytes(archery_data), uid, char_uid)
                    for uid in archery_uids
                }
            )
            archery_bounces.clear()
            archery_bounces.update(
                {
                    uid: container_packets(bytes(archery_data), uid, 0x40000004)
                    for uid in archery_uids[:2]
                }
            )

            # Discard the login snapshot, then issue the same legacy pickup and
            # equip requests used by a 3.0.6 client.
            drain(sock)
            # A trigger can remove the item it is equipping.  Stock still emits
            # the transient equipment packet before the delete; this is the
            # failing-first regression for the deferred equip update path.
            sock.sendall(struct.pack(">BIH", 0x07, self_remove_uid, 0))
            drain(sock, 0.25)
            sock.sendall(struct.pack(">BIBI", 0x13, self_remove_uid, 1, char_uid))
            self_response = bytearray()
            self_deadline = time.monotonic() + 2.0
            while time.monotonic() < self_deadline:
                self_response.extend(drain(sock, 0.2))
                self_remove_rows[:] = equipped_packets(bytes(self_response), self_remove_uid, char_uid)
                if self_remove_rows:
                    break

            sock.sendall(struct.pack(">BIH", 0x07, item_uid, 0))
            drain(sock, 0.25)
            sock.sendall(struct.pack(">BIBI", 0x13, item_uid, 1, char_uid))
            response = bytearray()
            deadline = time.monotonic() + 3.0
            while time.monotonic() < deadline:
                response.extend(drain(sock, 0.2))
                equip_rows[:] = equipped_packets(bytes(response), item_uid, char_uid)
                if equip_rows and time.monotonic() + 0.4 >= deadline:
                    # Keep a short bounded window for a second update emitted by
                    # the same request; the assertion must see every response.
                    time.sleep(0.4)
                    response.extend(drain(sock, 0.2))
                    equip_rows[:] = equipped_packets(bytes(response), item_uid, char_uid)
                    break
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
    if len(equip_rows) != 1:
        failures.append(
            f"one equip request emitted {len(equip_rows)} equipped-item packet(s)"
        )
    if len(self_remove_rows) != 1:
        failures.append(
            "self-removing equip emitted "
            f"{len(self_remove_rows)} equipped-item packet(s)"
        )
    if archery_layers is None:
        failures.append("archery marker was not recorded")
    else:
        one_uid, shield_uid, bow_uid, hand1_uid, hand2_uid = archery_layers
        if hand1_uid != 0:
            failures.append(f"two-handed equip left hand 1 occupied by 0x{hand1_uid:x}")
        if hand2_uid != bow_uid:
            failures.append(
                "two-handed equip did not occupy hand 2 with bow: "
                f"0x{hand2_uid:x} != 0x{bow_uid:x}"
            )
        if len(archery_equips.get(bow_uid, ())) != 1:
            failures.append(
                "two-handed script equip emitted "
                f"{len(archery_equips.get(bow_uid, ())) } bow equipment packet(s)"
            )
        for label, uid in (("one-handed weapon", one_uid), ("shield", shield_uid)):
            if not archery_bounces.get(uid):
                failures.append(
                    f"two-handed equip emitted no backpack packet for displaced {label}"
                )
    if failures:
        print("equip-packet probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "equip-packet probe passed: client equip emitted one packet and script bow equip "
        "emitted one bow packet with both displaced items moved to the backpack"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
