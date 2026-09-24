#!/usr/bin/env python3
"""Exercise equipped-item deletion during a character OnTick content walk."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT_NAME = "OnTickContentMutationListener"
LOGIN_VALUE = "ontick-content-mutation-pw"
UID_ORDER = (400, 401, 0x40000000 | 430, 0x40000000 | 431, 0x40000000 | 432)
EXPECTED_AFTER = (1, 0, 1, 0, 1)
REQUIRED_MARKERS = (
    "SPHERE_ONTICK_MUTATOR_TIMER",
    "SPHERE_ONTICK_MUTATOR_RETURNED",
    "SPHERE_ONTICK_SIBLING_UNEQUIP",
    "SPHERE_ONTICK_LISTENER_ALIVE",
    "SPHERE_ONTICK_UIDS_AFTER",
)


def all_system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def collect_markers(sock: socket.socket, initial: bytes, timeout: float) -> bytes:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    data = bytearray(initial)
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        data.extend(chunk)
        messages = []
        for packet in split_packet_stream(
            decode_game_response(bytes(data)), allow_truncated=True
        ):
            if packet.command == 0x1C and len(packet.data) >= 45:
                messages.append(
                    packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
                )
        if all(
            any(message == prefix or message.startswith(prefix + " ") for message in messages)
            for prefix in REQUIRED_MARKERS
        ):
            break
    return bytes(data)


def parse_bits(messages: list[str], prefix: str) -> tuple[int, ...] | None:
    marker = next((message for message in messages if message.startswith(prefix)), None)
    if marker is None:
        return None
    match = re.fullmatch(
        rf"{re.escape(prefix)} ([01](?:\|[01]){{{len(UID_ORDER) - 1}}})", marker
    )
    return tuple(int(value) for value in match.group(1).split("|")) if match else None


def marker_count(messages: list[str], prefix: str) -> int:
    return sum(message == prefix or message.startswith(prefix + " ") for message in messages)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2797)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT_NAME,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("listener did not reach the character list")
        try:
            sock.sendall(
                make_char_create(
                    name=ACCOUNT_NAME,
                    sex=0,
                    start_loc=1,
                    skill1=25,
                    val1=40,
                    skill2=26,
                    val2=40,
                    skill3=1,
                    val3=20,
                )
            )
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("listener did not enter the world")
            messages = all_system_messages(collect_markers(sock, response, 22.0))
            observed = parse_bits(messages, "SPHERE_ONTICK_UIDS_AFTER")
            if observed != EXPECTED_AFTER:
                failures.append(
                    f"OnTick mutation UIDs differed after cleanup: {observed!r}"
                )
            for prefix in REQUIRED_MARKERS:
                count = marker_count(messages, prefix)
                if count != 1:
                    failures.append(f"OnTick marker {prefix} occurred {count} times")
            if failures:
                failures.append(f"observed synthetic messages: {messages!r}")
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
    if failures:
        print("OnTick content mutation probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "OnTick content mutation probe passed: "
        f"after_uids={EXPECTED_AFTER!r} markers_once={len(REQUIRED_MARKERS)}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
