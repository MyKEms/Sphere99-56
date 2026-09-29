#!/usr/bin/env python3
"""Check the legacy NEWEQUIP create-and-equip command."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "NewEquipProbe"
PASSWORD = "newequip-pw"
END_MARKER = "SPHERE_NEWEQUIP_END"
EQUIP_MARKER = "SPHERE_NEWEQUIP_EQUIP"
INVALID_RE = re.compile(r"^SPHERE_NEWEQUIP_INVALID (.*)$")
VALID_RE = re.compile(r"^SPHERE_NEWEQUIP_VALID (\d+)\|(\d+)$")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2870)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("NEWEQUIP probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("NEWEQUIP probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 12.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            messages[:] = system_messages(bytes(data))
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

    invalid = [match.group(1) for message in messages if (match := INVALID_RE.fullmatch(message))]
    valid = [match.groups() for message in messages if (match := VALID_RE.fullmatch(message))]
    if messages.count(EQUIP_MARKER) != 1:
        failures.append(f"equip callback count: {messages.count(EQUIP_MARKER)}")
    if not valid or valid[0][0] == "0" or valid[0][0] != valid[0][1]:
        failures.append(f"valid NEWEQUIP result: {valid!r}")
    elif invalid != [valid[0][1]]:
        failures.append(f"invalid NEWEQUIP changed the equipped item: {invalid!r}")
    if END_MARKER not in messages:
        failures.append("NEWEQUIP probe did not reach its end marker")

    total = 4
    if failures:
        print(f"NEWEQUIP probe failed: {max(0, total - len(failures))}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"NEWEQUIP probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
