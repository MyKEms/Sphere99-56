#!/usr/bin/env python3
"""Check same-faction and outcast reactions from ``@NPCSeeNewPlayer``."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path

from modes.faction_guard import (
    OUTCAST_ACCOUNT,
    OUTCAST_MARKER,
    OUTCAST_PASSWORD,
    SAME_ACCOUNT,
    SAME_MARKER,
    SAME_PASSWORD,
)
from run_suite import shutdown_failures


TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _text(packet) -> str:
    if packet.command != 0x1C or len(packet.data) < 45:
        return ""
    return packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")


def _enter(host: str, port: int, account: str, password: str) -> socket.socket:
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    last_error = "character list was unavailable"
    for _attempt in range(4):
        sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
        if sock is None:
            last_error = "character list was unavailable"
            time.sleep(1.0)
            continue
        sock.sendall(make_char_play(0))
        if recv_until_game_start(sock, timeout=30.0):
            return sock
        sock.close()
        last_error = "character did not enter the world"
        time.sleep(1.0)
    raise RuntimeError(f"{account} {last_error}")


def _collect(sock: socket.socket, seconds: float) -> list:
    data = bytearray()
    deadline = time.monotonic() + seconds
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
    return _decode(bytes(data))


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3200)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    observed: dict[str, dict[str, object]] = {}
    sockets: list[socket.socket] = []

    from test_world_save_roundtrip import run_server

    def exercise() -> None:
        cases = (
            ("same", SAME_ACCOUNT, SAME_PASSWORD, SAME_MARKER, False),
            ("outcast", OUTCAST_ACCOUNT, OUTCAST_PASSWORD, OUTCAST_MARKER, True),
        )
        for label, account, password, marker, expects_attack in cases:
            sock = _enter(args.host, args.port, account, password)
            sockets.append(sock)
            packets = _collect(sock, 8.0)
            texts = [_text(packet) for packet in packets if _text(packet)]
            animations = sum(packet.command == 0x2F for packet in packets)
            observed[label] = {"texts": texts, "animations": animations}
            if marker not in texts:
                failures.append(f"{label} guard did not emit {marker}")
            attack_text = any("attacking" in text.casefold() for text in texts)
            if expects_attack and not attack_text:
                failures.append("outcast guard did not start an attack")
            if not expects_attack and (attack_text or animations):
                failures.append(
                    f"same-faction guard attacked (texts={texts!r}, animations={animations})"
                )
            sock.close()
            sockets.pop()
            time.sleep(2.0)

    try:
        returncode, runner_error, log_contents = run_server(
            fixture=fixture,
            binary=binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=fixture / "server.log",
            action=exercise,
        )
    except (OSError, RuntimeError, ValueError) as error:
        returncode, runner_error, log_contents = None, str(error), ""
    finally:
        for sock in sockets:
            try:
                sock.close()
            except OSError:
                pass
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    if failures:
        print(f"observed: {observed!r}", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "faction guard probe passed: same-faction no attack; outcast attack; "
        f"observed={observed!r}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
