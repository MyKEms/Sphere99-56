#!/usr/bin/env python3
"""Check pack-safe destination resolution for a script-created kit item."""

from __future__ import annotations

import argparse
from collections import Counter
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "StartingKitProbe"
PASSWORD = "kit-pw"
MARKER = "SPHERE_STARTING_KIT"
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")


def _messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    result: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        result.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return result


def _parse_value(messages: list[str], name: str) -> int | None:
    prefix = f"{MARKER}_{name} "
    for message in messages:
        if not message.startswith(prefix):
            continue
        value = message[len(prefix) :].strip().lstrip("#")
        try:
            return int(value, 16)
        except ValueError:
            try:
                return int(value, 10)
            except ValueError:
                return None
    return None


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2970)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("starting-kit probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("starting-kit probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 10.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = _messages(bytes(data))
                if f"{MARKER}_END" in messages:
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
            messages[:] = _messages(bytes(data))
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if f"{MARKER}_END" not in messages:
        failures.append("starting-kit probe did not reach its end marker")

    marker_counts = Counter(
        message.split(" ", 1)[0]
        for message in messages
        if message.startswith(f"{MARKER}_")
    )
    for marker in ("CHAR", "PACK", "ITEM", "PARENT", "TOP", "END"):
        marker_name = f"{MARKER}_{marker}"
        if marker_counts[marker_name] != 1:
            failures.append(
                f"{marker_name} marker count {marker_counts[marker_name]} != 1"
            )

    values = {name: _parse_value(messages, name) for name in ("CHAR", "PACK", "ITEM", "PARENT", "TOP")}
    if not values["CHAR"]:
        failures.append(f"character UID missing: {values['CHAR']!r}")
    if not values["PACK"]:
        failures.append(f"recreated backpack UID missing: {values['PACK']!r}")
    if values["PACK"] and values["PACK"] == values["CHAR"]:
        failures.append("recreated backpack resolved to the character UID")
    if not values["ITEM"]:
        failures.append(f"starting-kit item UID missing: {values['ITEM']!r}")
    if values["ITEM"] and values["ITEM"] == values["CHAR"]:
        failures.append("starting-kit item resolved to the character UID")
    if values["PACK"] and values["PARENT"] != values["PACK"]:
        failures.append(f"starting-kit parent {values['PARENT']!r} != backpack {values['PACK']!r}")
    if values["TOP"] and values["TOP"] != values["CHAR"]:
        failures.append(f"starting-kit top object {values['TOP']!r} != character {values['CHAR']!r}")
    if re.search(r"(?im)invalid container\s+0+\b", log_contents):
        failures.append("server log contains Invalid container 00")
    failures.extend(
        f"server log contains sanitizer output: {line}"
        for line in log_contents.splitlines()
        if any(marker in line for marker in SANITIZER_MARKERS)
    )

    if failures:
        print("starting-kit probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("starting-kit probe passed: recreated backpack owns the generated item")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
