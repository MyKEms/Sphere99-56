#!/usr/bin/env python3
"""Check handled RET_DEFAULT timer callbacks without misleading diagnostics."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import (
    TIMER_DEFAULT_REMOVE_ITEM_UID,
    TIMER_DEFAULT_REMOVE_MARKER,
    TIMER_DEFAULT_REMOVE_AFTER_MARKER,
    TIMER_DEFAULT_HANDLER_MARKER,
)
from run_suite import shutdown_failures


ACCOUNT_NAME = "TimerDefaultRemoveListener"
LOGIN_VALUE = "timer-default-remove-pw"
TIMER_ERROR = "Timer expired without DECAY flag 'synthetic timer default remove'?"
TIMER_HANDLER_ERROR = (
    "Timer expired without DECAY flag 'synthetic timer default handler'?"
)
TIMER_REMOVAL_RE = re.compile(
    rf"timer removed object uid=0x{TIMER_DEFAULT_REMOVE_ITEM_UID:x} reason=script\b",
    re.IGNORECASE,
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def collect_markers(sock: socket.socket, initial: bytes, timeout: float) -> list[str]:
    from uo_test_client import decode_game_response
    from uo_packets import split_packet_stream

    data = bytearray(initial)
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        messages = system_messages(bytes(data))
        if (
            any(message.startswith(TIMER_DEFAULT_REMOVE_AFTER_MARKER) for message in messages)
            and any(message.startswith(TIMER_DEFAULT_HANDLER_MARKER) for message in messages)
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
    list(split_packet_stream(decode_game_response(bytes(data)), allow_truncated=True))
    return system_messages(bytes(data))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2803)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--timer-timeout", type=float, default=30.0)
    args = parser.parse_args()

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import (
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
            raise RuntimeError("timer default-remove probe did not reach the character list")
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
                raise RuntimeError("timer default-remove probe did not enter the world")
            observed.extend(collect_markers(sock, response, args.timer_timeout))
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

    trigger_values = [
        message.split()[-1]
        for message in observed
        if message.startswith(TIMER_DEFAULT_REMOVE_MARKER)
    ]
    after_values = [
        message.split()[-1]
        for message in observed
        if message.startswith(TIMER_DEFAULT_REMOVE_AFTER_MARKER)
    ]
    handler_values = [
        message.split()[-1]
        for message in observed
        if message.startswith(TIMER_DEFAULT_HANDLER_MARKER)
    ]
    if trigger_values != ["1"]:
        failures.append(f"timer callback marker values were {trigger_values!r}")
    if after_values != ["0"]:
        failures.append(f"timer removal marker values were {after_values!r}")
    if handler_values != ["1"]:
        failures.append(f"default-handler marker values were {handler_values!r}")
    if log_contents.count(TIMER_ERROR):
        failures.append(
            f"self-removed timer emitted {log_contents.count(TIMER_ERROR)} generic timer error(s)"
        )
    if log_contents.count(TIMER_HANDLER_ERROR):
        failures.append(
            "handled default timer emitted "
            f"{log_contents.count(TIMER_HANDLER_ERROR)} generic timer error(s)"
        )
    if "world load: created_items=2 created_chars=1" not in log_contents:
        failures.append("saved timer fixture did not load both items and its owner")
    removal_markers = TIMER_REMOVAL_RE.findall(log_contents)
    if len(removal_markers) != 1:
        failures.append(
            "self-removing timer did not emit exactly one script-removal marker "
            f"(saw {len(removal_markers)})"
        )
    if failures:
        failures.append(f"observed system messages: {observed!r}")

    if failures:
        print("timer-default-remove probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1
    print(
        "timer-default-remove probe passed: RET_DEFAULT self-removal was handled "
        "once without the generic timer diagnostic"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
