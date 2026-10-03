#!/usr/bin/env python3
"""Check that the self GM command changes state and reports the result."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from modes.gm_toggle import (
    ACCOUNT,
    DAMAGE_ITEM_UID,
    PASSWORD,
)
from run_suite import shutdown_failures, stop_server, tail, wait_for_port

EXPECTED_TEXT = {
    "0": "'GM 0' is now set to '0'",
    "1": "'GM 1' is now set to '1'",
}
EXPECTED_HITS = {"0": "GM_TOGGLE_HITS 90", "1": "GM_TOGGLE_HITS 100"}
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")


def _decode(data: bytes):
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    return split_packet_stream(decode_game_response(data), allow_truncated=True)


def _talk(text: str) -> bytes:
    encoded = text.encode("ascii") + b"\0"
    return struct.pack(">BHBHH", 0x03, 8 + len(encoded), 0, 0, 3) + encoded


def _text(packet) -> str:
    if packet.command != 0x1C or len(packet.data) < 45:
        return ""
    return packet.data[44:].split(b"\0", 1)[0].decode("latin1", errors="replace")


def _drain(sock: socket.socket, timeout: float = 1.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
    finally:
        sock.setblocking(True)
    return bytes(data)


def _recv_until(sock: socket.socket, predicate, timeout: float = 5.0) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
            if predicate(_decode(bytes(data))):
                return bytes(data)
    finally:
        sock.setblocking(True)
    return bytes(data)


def _first_step(sock: socket.socket) -> None:
    """Release the login hook before exercising commands.

    The production login path does not process commands until the character
    has acknowledged its first walk.  Keep that state transition in this
    fixture so a missing GM response is an engine assertion, not a synthetic
    login artefact.
    """
    response = _walk(sock, 6, 1) + _walk(sock, 6, 2)
    if not any(packet.command == 0x22 for packet in response):
        raise RuntimeError("GM toggle fixture first walk was not acknowledged")


def _enter(port: int) -> socket.socket:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect("127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("GM toggle fixture did not reach the character list")
    sock.sendall(make_char_play(0))
    initial = recv_until_game_start(sock, timeout=10.0)
    if not initial:
        sock.close()
        raise RuntimeError("GM toggle fixture character did not enter the world")
    _drain(sock, 0.5)
    _first_step(sock)
    _drain(sock, 0.5)
    return sock


def _command(sock: socket.socket, value: str) -> tuple[list[str], list[int]]:
    sock.sendall(_talk(f".GM {value}"))
    packets = _decode(_drain(sock, 2.0))
    return [text for packet in packets if (text := _text(packet))], [packet.command for packet in packets]


def _click(sock: socket.socket) -> tuple[list[str], list[int]]:
    sock.sendall(struct.pack(">BI", 0x06, DAMAGE_ITEM_UID))
    packets = _decode(_recv_until(sock, lambda decoded: any(
        EXPECTED_HITS[value] in [_text(packet) for packet in decoded]
        for value in ("0", "1")
    ), timeout=4.0))
    return [text for packet in packets if (text := _text(packet))], [packet.command for packet in packets]


def _walk(sock: socket.socket, direction: int, sequence: int):
    sock.sendall(struct.pack(">BBBI", 0x02, direction, sequence, 0))
    packets = _decode(_recv_until(
        sock,
        lambda decoded: any(packet.command in (0x21, 0x22) for packet in decoded),
        timeout=4.0,
    ))
    return packets


def run_probe(port: int) -> list[str]:
    failures: list[str] = []
    sock = None
    try:
        sock = _enter(port)
        texts, commands = _command(sock, "0")
        if EXPECTED_TEXT["0"] not in texts:
            failures.append(
                f"GM 0 response missing {EXPECTED_TEXT['0']!r}; texts={texts!r}, "
                f"packets={[hex(c) for c in commands]!r}"
            )

        texts, _ = _click(sock)
        if EXPECTED_HITS["0"] not in texts:
            failures.append(f"GM 0 did not allow damage: texts={texts!r}")

        texts, commands = _command(sock, "1")
        if EXPECTED_TEXT["1"] not in texts:
            failures.append(
                f"GM 1 response missing {EXPECTED_TEXT['1']!r}; texts={texts!r}, "
                f"packets={[hex(c) for c in commands]!r}"
            )
        if 0x20 not in commands:
            failures.append(f"GM 1 did not publish the self mode update: packets={[hex(c) for c in commands]!r}")
        texts, _ = _click(sock)
        if EXPECTED_HITS["1"] not in texts:
            failures.append(f"GM 1 was not invulnerable: texts={texts!r}")

        texts, commands = _command(sock, "0")
        if EXPECTED_TEXT["0"] not in texts:
            failures.append(
                f"GM 0 reset response missing {EXPECTED_TEXT['0']!r}; texts={texts!r}, "
                f"packets={[hex(c) for c in commands]!r}"
            )
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        failures.append(str(error))
    finally:
        if sock is not None:
            sock.close()
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2940)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    log_path = fixture / "server.log"
    failures: list[str] = []
    process = None
    returncode = None
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, args.startup_timeout)
            failures.extend(run_probe(args.port))
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, log_contents))
    failures.extend(
        f"server log contains sanitizer output: {line}"
        for line in log_contents.splitlines()
        if any(marker in line for marker in SANITIZER_MARKERS)
    )
    if failures:
        print("GM toggle probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1
    print(
        "GM toggle probe passed: responses, mode update, and damage immunity"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
