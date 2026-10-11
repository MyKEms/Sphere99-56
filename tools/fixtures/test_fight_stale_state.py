#!/usr/bin/env python3
"""Verify that a callback-cancelled combat stroke is aborted safely."""

from __future__ import annotations

import argparse
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path

from modes.combat_scheduler import ACCOUNT, PASSWORD, TARGET_SERIAL
from run_suite import shutdown_failures, stop_server, wait_for_port


def _recv(sock: socket.socket, timeout: float) -> bytes:
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


def _enter(port: int) -> socket.socket:
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect(
        "127.0.0.1", port, ACCOUNT, PASSWORD, game_port=port + 1000
    )
    if sock is None:
        raise RuntimeError("fight stale-state fixture did not reach the character list")
    sock.sendall(make_char_play(0))
    if not recv_until_game_start(sock, timeout=20.0):
        sock.close()
        raise RuntimeError("fight stale-state fixture character did not enter the world")
    _recv(sock, 0.5)
    return sock


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=3002)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    failures: list[str] = []
    process = None
    returncode = None
    try:
        with (fixture / "server.log").open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, args.startup_timeout)
            sock = _enter(args.port)
            try:
                # Start a normal swing. The fixture callback clears ACTION
                # during @Stroke; the old code then dispatched the captured
                # combat stage into Fight_HitTry with no active fight.
                sock.sendall(bytes((0x72, 1, 0, 0x32, 0)))
                _recv(sock, 0.5)
                sock.sendall(struct.pack(">BI", 0x05, TARGET_SERIAL))
                _recv(sock, 0.5)
                time.sleep(4.0)
            finally:
                sock.close()
    except (OSError, RuntimeError, ValueError, struct.error) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = (fixture / "server.log").read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, log_contents))
    if "Exception in Sector" in log_contents:
        failures.append("callback-cancelled combat stroke raised a sector exception")

    if failures:
        print("fight stale-state probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("fight stale-state probe passed: combat callback cancellation aborted the stale stroke")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
