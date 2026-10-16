#!/usr/bin/env python3
"""Check that one equip request emits one equipped-item packet."""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from modes.equip_packet import ACCOUNT, LOGIN_CODE, MARKER
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

    def exercise() -> None:
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
            if len(fields) < 2:
                raise RuntimeError(f"malformed ready marker: {ready!r}")
            start = find_start_packet(decode_game_response(bytes(initial)))
            if start is None:
                raise RuntimeError("equip-packet probe did not receive a start packet")
            item_uid = parse_serial(fields[1])
            char_uid = struct.unpack_from(">I", start[1], 1)[0]

            # Discard the login snapshot, then issue the same legacy pickup and
            # equip requests used by a 3.0.6 client.
            drain(sock)
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
    if failures:
        print("equip-packet probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("equip-packet probe passed: one equip request emitted one equipped-item packet")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
