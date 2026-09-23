#!/usr/bin/env python3
"""Exercise timer-owner teardown and verify UID/count cleanup after GC."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT_NAME = "TimerLifetimeListener"
LOGIN_VALUE = "timer-lifetime-pw"
PREFIXES = (
    "SPHERE_TIMER_OBSERVER_CREATED",
    "SPHERE_TIMER_OBSERVER_EQUIPPED",
    "SPHERE_TIMER_OBSERVER_ARMED",
    "SPHERE_TIMER_COUNTS_BEFORE",
    "SPHERE_TIMER_UIDS_BEFORE",
    "SPHERE_TIMER_REMOVE_RETURNED",
    "SPHERE_TIMER_UNEQUIP_TRIGGERED",
    "SPHERE_TIMER_UNEQUIP_REMOVE_RETURNED",
    "SPHERE_TIMER_LISTENER_ALIVE",
)


def find_system_message(data: bytes, prefix: str) -> str | None:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    for packet in split_packet_stream(decode_game_response(data)):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        if text.startswith(prefix):
            return text
    return None


def all_system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def marker_count(messages: list[str], prefix: str) -> int:
    return sum(message.startswith(prefix) for message in messages)


def collect_markers(
    sock: socket.socket,
    initial: bytes,
    timeout: float,
    required_prefixes: tuple[str, ...] = PREFIXES,
) -> bytes:
    data = bytearray(initial)
    deadline = __import__("time").monotonic() + timeout
    sock.settimeout(0.2)
    while __import__("time").monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        data.extend(chunk)
        if all(find_system_message(data, prefix) for prefix in required_prefixes):
            break
    return bytes(data)


def parse_pair(messages: bytes, prefix: str) -> tuple[int, int] | None:
    marker = find_system_message(messages, prefix)
    if marker is None:
        return None
    match = re.fullmatch(rf"{re.escape(prefix)} (\d+)\|(\d+)", marker)
    return (int(match.group(1)), int(match.group(2))) if match else None


def parse_uids(messages: bytes, prefix: str) -> tuple[int, ...] | None:
    marker = find_system_message(messages, prefix)
    if marker is None:
        return None
    match = re.fullmatch(rf"{re.escape(prefix)} ([01](?:\|[01]){{5}})", marker)
    return tuple(int(value) for value in match.group(1).split("|")) if match else None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2794)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument(
        "--item-first",
        action="store_true",
        help="use the bounded item-first reentrant-removal acceptance window",
    )
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
            messages = collect_markers(
                sock,
                response,
                10.0 if args.item_first else 26.0,
                PREFIXES + ("SPHERE_TIMER_COUNTS_AFTER", "SPHERE_TIMER_UIDS_AFTER"),
            )
            observed_messages = all_system_messages(messages)

            before = parse_pair(messages, "SPHERE_TIMER_COUNTS_BEFORE")
            if before is None:
                failures.append("timer callback did not report pre-teardown counts")
            elif before[0] < 5 or before[1] < 2:
                failures.append(f"pre-teardown counts did not include the fixture tree: {before!r}")

            before_uids = parse_uids(messages, "SPHERE_TIMER_UIDS_BEFORE")
            if before_uids != (1, 1, 1, 1, 1, 1):
                failures.append(f"pre-teardown UID registry was not complete: {before_uids!r}")

            after = parse_pair(messages, "SPHERE_TIMER_COUNTS_AFTER")
            if after is None:
                failures.append("observer did not report post-GC counts")
            elif before is not None:
                item_delta = before[0] - after[0]
                char_delta = before[1] - after[1]
                if item_delta < 5 or char_delta < 1:
                    failures.append(f"teardown changed counts by {(item_delta, char_delta)!r}")

            after_uids = parse_uids(messages, "SPHERE_TIMER_UIDS_AFTER")
            if after_uids != (0, 0, 0, 0, 0, 0):
                failures.append(f"post-GC UID registry retained deleted objects: {after_uids!r}")

            for prefix in (
                "SPHERE_TIMER_LIFETIME_TRIGGERED",
                "SPHERE_TIMER_REMOVE_RETURNED",
                "SPHERE_TIMER_UNEQUIP_TRIGGERED",
                "SPHERE_TIMER_UNEQUIP_REMOVE_RETURNED",
                "SPHERE_TIMER_COUNTS_BEFORE",
                "SPHERE_TIMER_UIDS_BEFORE",
                "SPHERE_TIMER_COUNTS_AFTER",
                "SPHERE_TIMER_UIDS_AFTER",
                "SPHERE_TIMER_LISTENER_ALIVE",
            ):
                count = marker_count(observed_messages, prefix)
                if count != 1:
                    failures.append(f"timer marker {prefix} occurred {count} times")
            if find_system_message(messages, "SPHERE_TIMER_SIBLING_TRIGGERED") is not None:
                failures.append("deleted sibling timer ran after owner teardown")
            if failures:
                failures.append(f"observed synthetic messages: {observed_messages!r}")
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
        print("timer-lifetime probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("timer-lifetime probe passed: owner, nested children, and sibling left no UID entries")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
