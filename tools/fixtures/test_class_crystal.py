#!/usr/bin/env python3
"""Check protected crystal attributes and its scripted ability trigger."""

from __future__ import annotations

import argparse
import re
import socket
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT = "ClassCrystalProbe"
PASSWORD = "crystal-pw"
END_MARKER = "SPHERE_CLASS_CRYSTAL_END"
CLICK_MARKER = "SPHERE_CLASS_CRYSTAL_CLICK"
RESULT_RE = re.compile(r"^SPHERE_CLASS_CRYSTAL attr=(\S+) type=(\S+) click=(\d+) counter=(\d+)$")


def _messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    result: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        result.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return result


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4632)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    tools_path = Path(__file__).resolve().parents[1]
    import sys

    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("class-crystal probe did not reach the character list")
        try:
            sock.sendall(make_char_play(0))
            initial = recv_until_game_start(sock, timeout=30.0)
            if not initial:
                raise RuntimeError("class-crystal character did not enter the world")
            data = bytearray(initial)
            messages[:] = _messages(bytes(data))
            deadline = time.monotonic() + 15.0
            sock.settimeout(0.25)
            while time.monotonic() < deadline:
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
                messages[:] = _messages(bytes(data))
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=args.fixture.resolve(),
        binary=args.binary.resolve(),
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=args.fixture / "server.log",
        action=exercise,
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    results = [match.groups() for message in messages if (match := RESULT_RE.fullmatch(message))]
    if not results:
        failures.append(f"class-crystal result marker missing; messages={messages!r}")
    else:
        attr_text, item_type, click, counter_text = results[-1]
        attr = int(attr_text, 16)
        click = int(click)
        counter = int(counter_text)
        if attr != 0x14:
            failures.append(f"protected crystal attr after create: expected 20, got {attr}")
        if item_type == 0:
            failures.append("protected crystal type was empty")
        if click != 1:
            failures.append(f"class-crystal trigger result: expected 1, got {click}")
        if counter == 0:
            failures.append("class-crystal ability did not create its counter")
    if CLICK_MARKER not in messages:
        failures.append("class-crystal trigger marker missing")
    if END_MARKER not in messages:
        failures.append("class-crystal probe did not reach its end marker")

    total = 5
    if failures:
        print(f"class-crystal probe failed: {max(0, total - len(failures))}/{total} checks passed")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("class-crystal probe passed: 5/5 checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
