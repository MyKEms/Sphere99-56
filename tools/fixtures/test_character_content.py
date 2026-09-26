#!/usr/bin/env python3
"""Verify a no-LAYER item whose CONT points directly at a character."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from make_fixture import (
    CHARACTER_CONTENT_ACCOUNT,
    CHARACTER_CONTENT_CHAR_SERIAL,
    CHARACTER_CONTENT_ITEM_SERIAL,
    CHARACTER_CONTENT_MARKER,
    CHARACTER_CONTENT_PASSWORD,
)
from run_suite import shutdown_failures


END_MARKER = f"{CHARACTER_CONTENT_MARKER}_END"
MARKER_RE = re.compile(r"^" + re.escape(CHARACTER_CONTENT_MARKER) + r"_(UID|PARENT) (.*)$")


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def wait_for_world_load(log_path: Path, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            if "world load:" in log_path.read_text(encoding="utf-8", errors="replace"):
                return
        except OSError:
            pass
        time.sleep(0.1)
    raise RuntimeError("server did not finish world load before the bounded timeout")


def run_probe(fixture: Path, binary: Path, host: str, port: int, timeout: float) -> tuple[int | None, str | None, str, list[str]]:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    messages: list[str] = []

    def exercise() -> None:
        wait_for_world_load(fixture / "server.log", timeout)
        sock, _ = game_connect(
            host,
            port,
            CHARACTER_CONTENT_ACCOUNT,
            CHARACTER_CONTENT_PASSWORD,
            game_port=port + 1000,
        )
        if sock is None:
            raise RuntimeError("character-content probe account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("character-content probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    return
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    break
                data.extend(chunk)
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=binary,
        host=host,
        port=port,
        startup_timeout=timeout,
        log_path=fixture / "server.log",
        action=exercise,
    )
    return returncode, runner_error, log_contents, messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2755)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    returncode, runner_error, log_contents, messages = run_probe(
        args.fixture.resolve(), args.binary.resolve(), args.host, args.port, args.startup_timeout
    )
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    rows = {}
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2).strip()
    if END_MARKER not in messages:
        failures.append("character-content probe did not reach its end marker")
    if "UID" not in rows:
        failures.append("no-LAYER item was not discoverable from the character")
    else:
        try:
            item_uid = int(rows["UID"], 16)
        except ValueError:
            item_uid = -1
        expected_uid = 0x40000000 | CHARACTER_CONTENT_ITEM_SERIAL
        if item_uid != expected_uid:
            failures.append(
                "no-LAYER item UID was not preserved: "
                f"got {rows['UID']!r}; expected 0x{expected_uid:x}"
            )
    if "PARENT" not in rows:
        failures.append("no-LAYER item did not report a character parent")
    else:
        try:
            parent_uid = int(rows["PARENT"], 16)
        except ValueError:
            parent_uid = -1
        if parent_uid != CHARACTER_CONTENT_CHAR_SERIAL:
            failures.append(
                "no-LAYER item parent was not preserved: "
                f"got {rows['PARENT']!r}; expected character serial "
                f"{CHARACTER_CONTENT_CHAR_SERIAL}"
            )
    if failures:
        print("character-content probe failed: " + "; ".join(failures), file=sys.stderr)
        print(log_contents[-4000:], file=sys.stderr)
        return 1
    print(
        "character-content probe passed: no-LAYER item "
        f"{rows['UID']} retained character parent {rows['PARENT']} "
        f"(serial {CHARACTER_CONTENT_ITEM_SERIAL})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
