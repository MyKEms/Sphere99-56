#!/usr/bin/env python3
"""Exercise a saved i_memory-style script timer and its handled return value."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import MEMORY_TIMER_ITEM_UID
from run_suite import shutdown_failures


ACCOUNT_NAME = "MemoryTimerListener"
LOGIN_VALUE = "memory-timer-pw"
TRIGGER_MARKER = "SPHERE_MEMORY_TIMER_TRIGGERED"
REMOVED_MARKER = "SPHERE_MEMORY_TIMER_REMOVED"
TIMER_ERROR = "Timer expired without DECAY flag 'created memory'?"


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def collect_markers(sock: socket.socket, initial: bytes, timeout: float) -> bytes:
    from uo_test_client import decode_game_response
    from uo_packets import split_packet_stream

    data = bytearray(initial)
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        messages = system_messages(bytes(data))
        if any(message.startswith(TRIGGER_MARKER) for message in messages) and any(
            message.startswith(REMOVED_MARKER) for message in messages
        ):
            break
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        data.extend(chunk)
    # Decode once here so malformed/truncated packet data cannot make the
    # marker assertions accidentally pass.
    list(split_packet_stream(decode_game_response(bytes(data)), allow_truncated=True))
    return bytes(data)


def marker_values(messages: list[str], prefix: str) -> list[int]:
    values: list[int] = []
    for message in messages:
        match = re.fullmatch(rf"{re.escape(prefix)} ([01])", message)
        if match:
            values.append(int(match.group(1)))
    return values


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2800)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--timer-timeout", type=float, default=30.0)
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
    observed: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT_NAME,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("memory timer probe did not reach the character list")
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
                raise RuntimeError("memory timer probe did not enter the world")
            observed.extend(system_messages(collect_markers(sock, response, args.timer_timeout)))
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

    trigger_values = marker_values(observed, TRIGGER_MARKER)
    removed_values = marker_values(observed, REMOVED_MARKER)
    if trigger_values != [1]:
        failures.append(f"memory timer callback marker values were {trigger_values!r}")
    if removed_values != [0]:
        failures.append(f"memory timer removal marker values were {removed_values!r}")
    if log_contents.count(TIMER_ERROR):
        failures.append(
            f"handled memory timer still emitted {log_contents.count(TIMER_ERROR)} generic timer error(s)"
        )
    if "world load: created_items=1 created_chars=1" not in log_contents:
        failures.append("saved memory timer fixture did not load its item and owner")
    if failures:
        print("memory-timer probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"- expected item uid={MEMORY_TIMER_ITEM_UID:#x}", file=sys.stderr)
        print(f"- observed messages: {observed!r}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1
    print(
        "memory-timer probe passed: callback/removal markers once, "
        "generic timer error absent, clean shutdown"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
