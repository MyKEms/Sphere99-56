#!/usr/bin/env python3
"""Check equal-Z ground-item triggers and object-valued ACT references."""

from __future__ import annotations

import argparse
import re
import socket
import struct
import sys
import time
from pathlib import Path

from modes.ground_item_click import ACCOUNT, LOGIN_VALUE, MARKER, SAME_Z_UID
from run_suite import shutdown_failures


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    result: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        result.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return result


def collect(sock: socket.socket, buffer: bytearray, timeout: float = 2.0) -> list[str]:
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        buffer.extend(chunk)
    return system_messages(bytes(buffer))


def wait_for(sock: socket.socket, buffer: bytearray, predicate, timeout: float = 5.0) -> list[str]:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        messages = system_messages(bytes(buffer))
        if predicate(messages):
            return messages
        messages = collect(sock, buffer, min(0.25, max(0.01, deadline - time.monotonic())))
        if predicate(messages):
            return messages
    return system_messages(bytes(buffer))


def run_probe(args: argparse.Namespace) -> int:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    failures: list[str] = []
    last_messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("ground-item click probe did not reach the character list")
        buffer = bytearray()
        try:
            sock.sendall(make_char_play(0))
            buffer.extend(recv_until_game_start(sock, timeout=30.0))
            if not buffer:
                raise RuntimeError("ground-item click character did not enter the world")
            messages = collect(sock, buffer, 0.5)
            if f"{MARKER}_TOO_FAR" not in messages:
                failures.append("height-different trigger was not refused")
            buffer.clear()

            # The equal-Z item must reach @ItemUserDClick without the height
            # guard rejecting it.  The failing-first master result is the
            # missing USED row (and an empty ACT reference).
            sock.sendall(struct.pack(">BI", 0x06, SAME_Z_UID))
            messages = wait_for(
                sock,
                buffer,
                lambda values: any(value.startswith(f"{MARKER}_USED") for value in values)
                or f"{MARKER}_TOO_FAR" in values,
            )
            if not any(value.startswith(f"{MARKER}_USED") for value in messages):
                failures.append("equal-Z double-click did not produce the USED marker")
            used_rows = [value for value in messages if value.startswith(f"{MARKER}_USED")]
            if used_rows and not re.match(
                rf"^{re.escape(MARKER)}_USED\|.+\|040000064\|10\|10$",
                used_rows[-1],
            ):
                failures.append(f"equal-Z USED marker lost the bare ACT reference: {used_rows[-1]!r}")

            # Pick up the equal-Z item and drop it back on the ground.  The
            # reflected @ItemDropon_Ground event must see the live ACT item,
            # including through itemExists(<ACT>).
            sock.sendall(struct.pack(">BIH", 0x07, SAME_Z_UID, 0))
            collect(sock, buffer, 0.4)
            sock.sendall(struct.pack(">BIHHBI", 0x08, SAME_Z_UID, 129, 128, 10, 0))
            messages = wait_for(
                sock,
                buffer,
                lambda values: any(value.startswith(f"{MARKER}_DROP") for value in values),
            )
            drop_rows = [value for value in messages if value.startswith(f"{MARKER}_DROP")]
            if not drop_rows:
                failures.append("ground drop did not produce the DROP marker")
            elif not re.match(rf"^{re.escape(MARKER)}_DROP\|1\|.+\|040000064\|10$", drop_rows[-1]):
                failures.append(f"ground drop did not preserve ACT item reference: {drop_rows[-1]!r}")
            last_messages[:] = messages
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=args.fixture.resolve(),
        binary=args.binary.resolve(),
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=args.fixture.resolve() / "server.log",
        action=exercise,
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print("ground-item click probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("messages:", last_messages, file=sys.stderr)
        return 1
    print("ground-item click probe passed: equal-Z use, height refusal, and ACT drop reference")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2900)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    return run_probe(args)


if __name__ == "__main__":
    raise SystemExit(main())
