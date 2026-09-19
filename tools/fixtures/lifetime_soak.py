#!/usr/bin/env python3
"""Exercise the bounded CClient lifetime contract against a disposable server."""

from __future__ import annotations

import socket
import struct
import sys
import time

from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from uo_packets import split_packet_stream
from uo_test_client import (
    decode_game_response,
    find_start_packet,
    game_connect,
    make_char_create,
)


def assert_server_alive(host: str, port: int) -> None:
    with socket.create_connection((host, port), timeout=3.0):
        return


def drain_nonblocking(sock: socket.socket) -> bytes:
    data = bytearray()
    sock.setblocking(False)
    try:
        while True:
            chunk = sock.recv(65536)
            if not chunk:
                break
            data.extend(chunk)
    except BlockingIOError:
        pass
    finally:
        sock.setblocking(True)
        sock.settimeout(5.0)
    return bytes(data)


def run_cycle(host: str, port: int, game_port: int, cycle: int) -> int:
    account = f"lifetime_{cycle}_{int(time.time())}_{abs(hash((host, cycle))) % 10000}"[:29]
    password = f"lifetime_pw_{cycle}"
    sock, _ = game_connect(host, port, account, password, game_port=game_port)
    if sock is None:
        raise RuntimeError(f"cycle {cycle}: login/charlist failed")

    try:
        sock.sendall(make_char_create(name=f"Soak{cycle}", sex=0, start_loc=1))
        from uo_test_client import recv_until_game_start

        entry = decode_game_response(recv_until_game_start(sock, timeout=10.0))
        if find_start_packet(entry) is None:
            raise RuntimeError(f"cycle {cycle}: no structurally valid game-start packet")

        # Walk a small bounded path and require the response stream to remain
        # structurally parseable. Collision rules may legitimately reject a
        # particular step, so the lifecycle assertion is server survival.
        for sequence in range(1, 4):
            sock.sendall(struct.pack(">BBBI", 0x02, sequence % 8, sequence, 0))
            time.sleep(0.1)
        time.sleep(0.2)
        movement = decode_game_response(drain_nonblocking(sock))
        if movement:
            split_packet_stream(movement, allow_truncated=True)

        # Alternate FIN-style close and RST-style close. Both paths must leave
        # the server able to accept a fresh connection on the next tick.
        if cycle % 2 == 0:
            sock.shutdown(socket.SHUT_RDWR)
            sock.close()
        else:
            sock.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER, struct.pack("ii", 1, 0))
            sock.close()
    finally:
        try:
            sock.close()
        except OSError:
            pass

    time.sleep(0.15)
    assert_server_alive(host, port)
    return len(movement)


def main() -> int:
    host = sys.argv[1] if len(sys.argv) > 1 else "127.0.0.1"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2593
    game_port = int(sys.argv[3]) if len(sys.argv) > 3 else port + 1000
    cycles = int(sys.argv[4]) if len(sys.argv) > 4 else 25
    if cycles < 1 or cycles > 100:
        raise SystemExit("cycles must be between 1 and 100")

    print(f"Client lifetime soak: {cycles} cycles on {host}:{port}/{game_port}")
    for cycle in range(1, cycles + 1):
        movement_bytes = run_cycle(host, port, game_port, cycle)
        print(f"  PASS: cycle {cycle}/{cycles} (movement response {movement_bytes} bytes)")
    print(f"Lifetime soak passed: {cycles}/{cycles} cycles")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
