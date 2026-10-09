#!/usr/bin/env python3
"""Check the unselected skills of a newly created player."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from modes.player_skill_defaults import (
    ACCOUNT_BY_LIMIT,
    CHARACTER_BY_LIMIT,
    END_MARKER,
    MARKER,
    PASSWORD,
)
from run_suite import shutdown_failures


MARKER_RE = re.compile(re.escape(MARKER) + r" \[(.*)\]$")


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
    parser.add_argument("--port", type=int, default=4620)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--max-base-skill", type=int, choices=tuple(ACCOUNT_BY_LIMIT))
    args = parser.parse_args()

    if args.max_base_skill is None:
        parser.error("--max-base-skill is required")

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    ini_path = fixture / "sphere.ini"
    ini = ini_path.read_text(encoding="ascii")
    ini = re.sub(
        r"(?im)^MAXBASESKILL=.*$",
        f"MAXBASESKILL={args.max_base_skill}",
        ini,
    )
    if "MAXBASESKILL=" not in ini.upper():
        ini += f"\nMAXBASESKILL={args.max_base_skill}\n"
    ini_path.write_text(ini, encoding="ascii")
    account = ACCOUNT_BY_LIMIT[args.max_base_skill]
    character = CHARACTER_BY_LIMIT[args.max_base_skill]
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server
    from uo_test_client import game_connect, make_char_create, recv_until_game_start

    messages: list[str] = []
    failures: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host, args.port, account, PASSWORD, game_port=args.port + 1000
        )
        if sock is None:
            raise RuntimeError("fresh-player probe did not reach its character list")
        try:
            sock.sendall(make_char_create(name=character, start_loc=1))
            data = bytearray(recv_until_game_start(sock, timeout=30.0))
            if not data:
                raise RuntimeError("fresh-player probe did not enter the world")
            deadline = time.monotonic() + 10.0
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
    if messages.count(END_MARKER) != 1:
        failures.append(f"end marker count {messages.count(END_MARKER)} != 1")

    observed: tuple[str, ...] | None = None
    for message in messages:
        match = MARKER_RE.fullmatch(message)
        if match:
            observed = tuple(part.strip() for part in match.group(1).split("|"))
            break
    if observed is None or len(observed) != 2:
        failures.append(f"fresh-player skill marker {observed!r} has wrong shape")
    else:
        try:
            evalint, resist = (int(value) for value in observed)
        except ValueError:
            failures.append(f"fresh-player skill marker is not numeric: {observed!r}")
        else:
            if not (0 <= evalint <= args.max_base_skill and 0 <= resist <= args.max_base_skill):
                failures.append(
                    f"unselected skills {(evalint, resist)!r} exceed configured "
                    f"range 0..{args.max_base_skill}"
                )
            if args.max_base_skill == 0 and (evalint, resist) != (0, 0):
                failures.append(
                    f"MAXBASESKILL=0 produced nonzero unselected skills {(evalint, resist)!r}"
                )

    if failures:
        print("player-skill-defaults probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(f"messages: {messages!r}", file=sys.stderr)
        return 1
    print(
        "player-skill-defaults probe passed: "
        f"MAXBASESKILL={args.max_base_skill} runtime/range checks"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
