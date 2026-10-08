#!/usr/bin/env python3
"""Check stock-compatible cleanup of unplaced script-created items."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures

from modes.unplaced_newitem import (
    ACCOUNT,
    MARKER,
    PASSWORD,
)


END_MARKER = MARKER + "_END"
MARKER_RE = re.compile(re.escape(MARKER) + r"_([A-Z]+) (.*)$")


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
    parser.add_argument("--port", type=int, default=3160)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []

    def saved_world_text() -> str:
        try:
            return (fixture / "save" / "sphereworld.scp").read_text(
                encoding="ascii", errors="replace"
            )
        except OSError:
            # The save publishes the pair by rename.  A poll that lands in
            # the small rotation window is not evidence that the item was
            # lost; keep polling until the bounded deadline.
            return ""

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT,
            PASSWORD,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("unplaced NEWITEM probe did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response:
                raise RuntimeError("unplaced NEWITEM probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 8.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                messages[:] = system_messages(bytes(data))
                if END_MARKER in messages:
                    # SERV.SAVE is asynchronous; wait until the placed item has
                    # been published before stopping the disposable server.
                    if "SYNTHETIC_PLACED_PROBE" in saved_world_text():
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
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")

    rows: dict[str, str] = {}
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            rows[match.group(1)] = match.group(2)
    if not rows.get("CREATED", "").endswith("|-1,-1,0"):
        failures.append(f"unplaced item did not start at (-1,-1,0): {rows.get('CREATED')!r}")
    if not rows.get("PLACED", "").endswith("|128,128,0"):
        failures.append(f"placed item did not receive its valid point: {rows.get('PLACED')!r}")
    if rows.get("LOOKUP") != "synthetic unplaced probe|synthetic placed probe":
        failures.append(f"items were not both visible during the trigger: {rows.get('LOOKUP')!r}")

    world_text = saved_world_text()
    if "SYNTHETIC_PLACED_PROBE" not in world_text:
        failures.append("validly placed item was not written by the save")
    if "SYNTHETIC_UNPLACED_PROBE" in world_text:
        failures.append("unplaced item was written by the save")
    if "Lost object deleted" in log_contents:
        failures.append("cleanup emitted the Linux-only Lost object deleted diagnostic")
    diagnostics = [
        line
        for line in log_contents.splitlines()
        if "world load diagnostics:" in line and "deleted=1" in line
    ]
    if len(diagnostics) != 1:
        failures.append(
            "expected one aggregate world-load deleted=1 diagnostic, "
            f"found {len(diagnostics)}"
        )

    if failures:
        print("unplaced NEWITEM probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print("unplaced NEWITEM probe passed: transient item was dropped and placed item survived")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
