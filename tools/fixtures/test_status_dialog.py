#!/usr/bin/env python3
"""Check the status packet emitted after a stat-changing dialog response."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from make_status_dialog_fixture import (
    ACCOUNT,
    BUTTON_ID,
    ITEM_UID,
    LOGIN_VALUE,
    MARKER,
    TEXT_LABEL,
)
from run_suite import shutdown_failures


def _message(packet: bytes) -> str | None:
    if packet[0] != 0x1C or len(packet) < 45:
        return None
    return packet[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")


def exercise(args: argparse.Namespace) -> list[str]:
    from uo_gumps import make_gump_reply, parse_gump_dialog
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
        args.host, args.port, ACCOUNT, LOGIN_VALUE, game_port=args.port + 1000
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
        start = find_start_packet(decode_game_response(response)) if response else None
        if start is None:
            return ["probe character did not enter the world"]
        self_uid = int.from_bytes(start[1][1:5], "big")
        raw.extend(response)

        def next_packet(timeout: float = 10.0):
            nonlocal packet_index
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                packets = split_packet_stream(
                    decode_game_response(bytes(raw)), allow_truncated=True
                )
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

        # Open the dialog from a world object after the login burst has
        # settled.  This matches the normal client path and ensures the
        # response is not still inside the login pause window.
        time.sleep(1.0)
        sock.sendall(bytes([0x06]) + ITEM_UID.to_bytes(4, "big"))
        gump = None
        while gump is None:
            packet = next_packet(20.0)
            if packet is None:
                failures.append("server did not open a stat-update dialog")
                return failures
            if packet.command in (0xB0, 0xDD):
                try:
                    gump = parse_gump_dialog(packet.data)
                except ValueError as error:
                    failures.append(f"malformed stat-update dialog: {error}")
                    return failures
        button = gump.find_button(button_id=BUTTON_ID)
        if button is None or button.button_id != BUTTON_ID:
            failures.append(f"dialog label {TEXT_LABEL!r} did not resolve to button {BUTTON_ID}")
            return failures

        sock.sendall(
            make_gump_reply(gump.serial, gump.context, button.button_id, switches=(0,))
        )
        marker = f"{MARKER}|{BUTTON_ID}|123|77|88"
        response_packets = []
        deadline = time.monotonic() + 10.0
        marker_index = None
        while time.monotonic() < deadline:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            response_packets.append(packet)
            if marker_index is None and _message(packet.data) == marker:
                marker_index = len(response_packets) - 1
            if marker_index is not None:
                break
        commands = [f"0x{packet.command:02x}" for packet in response_packets]
        if marker_index is None:
            failures.append(f"dialog reply marker {marker!r} missing; packets={commands!r}")

        # The normal client requests the refreshed self-status after the
        # dialog response.  Drain the response batch before marking the
        # request so an already queued status packet cannot satisfy it.
        while next_packet(0.5) is not None:
            pass
        status_start = packet_index
        sock.sendall(bytes([0x06]) + self_uid.to_bytes(4, "big"))
        status_packet = None
        deadline = time.monotonic() + 3.0
        while time.monotonic() < deadline:
            packet = next_packet(max(0.1, deadline - time.monotonic()))
            if packet is None:
                break
            if packet.command == 0x11:
                status_packet = packet
                break
        if status_packet is None:
            failures.append(
                "self-status request after stat dialog did not contain a 0x11 packet; "
                f"response_commands={commands!r} request_start={status_start}"
            )
        elif len(status_packet.data) < 50:
            failures.append("self-status response was truncated")
        else:
            values = tuple(int.from_bytes(status_packet.data[offset : offset + 2], "big") for offset in (44, 46, 48))
            if values != (123, 77, 88):
                failures.append(f"self-status response reported {values!r}, expected (123, 77, 88)")
    finally:
        sock.close()
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2894)
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
    from test_world_save_roundtrip import run_server

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
        print("status-dialog probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("status-dialog probe passed: stat update was emitted in the dialog response")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
