#!/usr/bin/env python3
"""Verify bounded escape expansion while an existing character logs in."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path

from make_fixture import (
    ESCAPE_OVERFLOW_ACCOUNT,
    ESCAPE_OVERFLOW_MARKER,
    ESCAPE_OVERFLOW_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


def drain_game_socket(sock: socket.socket, initial: bytes) -> bytes:
    data = bytearray(initial)
    deadline = time.monotonic() + 1.5
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
    except (ConnectionResetError, OSError):
        pass
    finally:
        sock.setblocking(True)
    return bytes(data)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0]
        messages.append(text.decode("ascii", errors="replace"))
    return messages


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2798)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    log_path = fixture / "server.log"
    failures: list[str] = []
    server_returncode = None
    response = b""

    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open server log: {error}")
        return 1

    with log_file:
        process = None
        try:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port(args.host, args.port, args.startup_timeout)
            sock, _ = game_connect(
                args.host,
                args.port,
                ESCAPE_OVERFLOW_ACCOUNT,
                ESCAPE_OVERFLOW_PASSWORD,
                game_port=args.port + 1000,
            )
            if sock is None:
                raise RuntimeError("existing account did not reach its character list")
            try:
                sock.sendall(make_char_play(0))
                response = recv_until_game_start(sock, timeout=30.0)
                response = drain_game_socket(sock, response)
            finally:
                sock.close()
        except (OSError, RuntimeError) as error:
            failures.append(str(error))
        finally:
            if process is not None:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")

    failures.extend(shutdown_failures(server_returncode, log_contents))
    decoded = decode_game_response(response)
    if not response or find_start_packet(decoded) is None:
        failures.append("existing character did not enter the world")
    if ESCAPE_OVERFLOW_MARKER not in system_messages(decoded):
        failures.append("login did not reach the post-expansion marker")
    if "Script escape expansion exceeds line buffer" not in log_contents:
        failures.append("bounded expansion diagnostic was not logged")

    if failures:
        print("login escape overflow probe failed:")
        for failure in failures:
            print(f"- {failure}")
        print("\n--- server log (tail) ---")
        print("\n".join(log_contents.splitlines()[-80:]))
        return 1

    print(
        "login escape overflow probe passed: existing character entered, "
        "post-expansion marker arrived, and the expansion was bounded"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
