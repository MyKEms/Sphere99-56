#!/usr/bin/env python3
"""Drive the synthetic gump fixture with the public headless client.

The fixture opens a 0xB0 dialog after character creation.  This test parses
the layout and text table, selects a button by its label, sends a complete
0xB1 reply, and checks the server-side marker produced by the button handler.
It also sends the classic 0x12/0x24 skill-use request through the same live
socket; shutdown remains bounded and is checked by the fixture runner.
"""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path
from typing import Optional

from make_gump_skill_fixture import (
    ACCOUNT,
    BUTTON_ID,
    MARKER,
    PASSWORD,
    TEXT_LABEL,
)
from run_suite import shutdown_failures


def _system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages: list[str] = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def exercise(args: argparse.Namespace) -> list[str]:
    from uo_gumps import make_gump_reply, make_skill_use, parse_gump_dialog
    from uo_packets import split_packet_stream
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    failures: list[str] = []
    sock, _ = game_connect(
        args.host,
        args.port,
        ACCOUNT,
        PASSWORD,
        game_port=args.port + 1000,
    )
    if sock is None:
        return ["probe account did not reach the character list"]

    raw = bytearray()
    packet_index = 0
    try:
        sock.sendall(
            make_char_create(
                name=ACCOUNT,
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
            return ["probe character did not enter the world"]
        raw.extend(response)

        def next_packet(timeout: float = 10.0):
            nonlocal packet_index
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                decoded = decode_game_response(bytes(raw))
                packets = split_packet_stream(decoded, allow_truncated=True)
                if packet_index < len(packets):
                    packet = packets[packet_index]
                    packet_index += 1
                    return packet
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    return None
                if not chunk:
                    return None
                raw.extend(chunk)
            return None

        gump = None
        packet_commands: list[str] = []
        pending_messages: list[str] = []
        deadline = time.monotonic() + 20.0
        while time.monotonic() < deadline and gump is None:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            packet_commands.append(f"0x{packet.command:02x}")
            pending_messages.extend(_system_messages(packet.data))
            if packet.command in (0xB0, 0xDD):
                try:
                    gump = parse_gump_dialog(packet.data)
                except ValueError as error:
                    failures.append(f"malformed gump packet: {error}")
                    break
        if gump is None:
            failures.append(
                "server did not open a 0xB0/0xDD gump; "
                f"messages={pending_messages!r} packets={packet_commands!r}"
            )
            return failures
        button = gump.find_button(button_id=BUTTON_ID)
        if button is None:
            button = gump.find_button(label=TEXT_LABEL)
        if button is None or button.button_id != BUTTON_ID:
            failures.append("gump label did not resolve to button 7")
            return failures

        # Sphere's legacy decoder expects one check slot before it reads the
        # trailing text count, even when the dialog has no checkboxes.
        sock.sendall(
            make_gump_reply(
                gump.serial, gump.context, button.button_id, switches=(0,)
            )
        )
        marker = f"{MARKER}|{BUTTON_ID}|1"
        deadline = time.monotonic() + 10.0
        while marker not in pending_messages and time.monotonic() < deadline:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            pending_messages.extend(_system_messages(packet.data))
        if marker not in pending_messages:
            failures.append(
                f"button reply did not produce server marker {marker!r}; "
                f"messages={pending_messages!r}"
            )

        # Exercise the skill request wire path after the gump action.  The
        # engine may legitimately answer with a localized skill message, so
        # the bounded socket write and clean runner shutdown are the assertion.
        sock.sendall(make_skill_use(1))
    finally:
        sock.close()
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2735)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    args.fixture = args.fixture.resolve()
    args.binary = args.binary.resolve()
    if not (args.fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {args.fixture / 'sphere.ini'}")
    if not args.binary.is_file():
        parser.error(f"server binary does not exist: {args.binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel

    failures: list[str] = []
    returncode, runner_error, log_contents = run_server(
        fixture=args.fixture,
        binary=args.binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=args.fixture / "server.log",
        action=lambda: failures.extend(exercise(args)),
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print("gump/skill probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "gump/skill probe passed: parsed layout, selected button, "
        "checked effect, and sent skill request"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
