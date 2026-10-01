#!/usr/bin/env python3
"""Verify script GM-command markers and authenticated console input."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

from make_fixture import (
    GM_COMMAND_LOG_ACCOUNT,
    GM_COMMAND_LOG_MARKERS,
    GM_COMMAND_LOG_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


COMMAND_LOG = re.compile(
    r"commands uid=0x?[0-9a-fA-F]+.* to 's\(([^']+)\)' OK",
    re.IGNORECASE,
)


def read_logs(fixture: Path) -> str:
    chunks: list[str] = []
    server_log = fixture / "server.log"
    if server_log.exists():
        chunks.append(server_log.read_text(encoding="utf-8", errors="replace"))
    for path in sorted((fixture / "logs").glob("sphere*.log")):
        try:
            chunks.append(path.read_text(encoding="utf-8", errors="replace"))
        except OSError:
            continue
    return "\n".join(chunks)


def recv_until(sock: socket.socket, needle: bytes, timeout: float = 5.0) -> bytes:
    deadline = time.monotonic() + timeout
    data = bytearray()
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
        if needle in data:
            break
    return bytes(data)


def run_telnet_probe(host: str, port: int) -> bytes:
    with socket.create_connection((host, port), timeout=5.0) as sock:
        sock.sendall(b" ")
        recv_until(sock, b"Username?:")
        sock.sendall(GM_COMMAND_LOG_ACCOUNT.encode("ascii") + b"\r")
        recv_until(sock, b"Password?:")
        sock.sendall(GM_COMMAND_LOG_PASSWORD.encode("ascii") + b"\r")
        welcome = recv_until(sock, b"Telnet")
        sock.sendall(b"?\r")
        command_list = recv_until(sock, b"Available Commands:")
        sock.sendall(b"B GM_CONSOLE_INPUT_MARKER\r")
        broadcast = recv_until(sock, b"GM_CONSOLE_INPUT_MARKER")
        return welcome + command_list + broadcast


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2884)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    telnet_output = b""

    with (fixture / "server.log").open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
            from uo_test_client import game_connect, make_char_play, recv_until_game_start

            sock, _ = game_connect(
                args.host,
                args.port,
                GM_COMMAND_LOG_ACCOUNT,
                GM_COMMAND_LOG_PASSWORD,
                game_port=args.port + 1000,
            )
            if sock is None:
                raise RuntimeError("command-log probe did not reach the character list")
            try:
                sock.sendall(make_char_play(0))
                recv_until_game_start(sock, timeout=30.0)
                time.sleep(1.0)
            finally:
                sock.close()

            telnet_output = run_telnet_probe(args.host, args.port)

            deadline = time.monotonic() + 15.0
            contents = ""
            while time.monotonic() < deadline:
                contents = read_logs(fixture)
                if (
                    all(marker in contents for marker in GM_COMMAND_LOG_MARKERS)
                ):
                    break
                time.sleep(0.1)
        except Exception as error:  # noqa: BLE001 - report fixture failure details
            failures.append(str(error))
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    contents = read_logs(fixture)
    failures.extend(shutdown_failures(returncode, contents))
    missing = [marker for marker in GM_COMMAND_LOG_MARKERS if marker not in contents]
    if missing:
        failures.append(f"missing {len(missing)} dispatched command marker(s): {missing!r}")
    if "Available Commands:" not in contents:
        if b"Available Commands:" not in telnet_output:
            failures.append("telnet '?' command did not produce its command list")
    if b"GM_CONSOLE_INPUT_MARKER" not in telnet_output:
        failures.append("telnet broadcast marker was not captured")
    command_lines = COMMAND_LOG.findall(contents)
    if len(command_lines) < len(GM_COMMAND_LOG_MARKERS):
        failures.append(
            f"captured {len(command_lines)} successful command-log records; "
            f"expected at least {len(GM_COMMAND_LOG_MARKERS)}"
        )
    total = len(GM_COMMAND_LOG_MARKERS) + 3
    passed = max(0, total - len(failures))
    if failures:
        print(f"GM command-log probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- logs ---", file=sys.stderr)
        print("\n".join(contents.splitlines()[-120:]), file=sys.stderr)
        return 1
    print(f"GM command-log probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
