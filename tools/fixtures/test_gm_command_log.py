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
    GM_COMMAND_LOG_PLAYER_ACCOUNT,
    GM_COMMAND_LOG_PLAYER_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


COMMAND_LOG = re.compile(
    r"commands uid=0x?[0-9a-fA-F]+.* to 's\(([^']+)\)' OK",
    re.IGNORECASE,
)
CONSOLE_INPUT_LIMIT = 8 * 1024 - 1


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


def recv_until_close(sock: socket.socket, timeout: float = 5.0) -> tuple[bytes, bool]:
    deadline = time.monotonic() + timeout
    data = bytearray()
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            continue
        if not chunk:
            return bytes(data), True
        data.extend(chunk)
    return bytes(data), False


def begin_telnet_login(host: str, port: int, username: str, password: str) -> tuple[socket.socket, bytes]:
    sock = socket.create_connection((host, port), timeout=5.0)
    output = bytearray()
    try:
        sock.sendall(b" ")
        prompt = recv_until(sock, b"Username?:")
        output.extend(prompt)
        if b"Username?:" not in prompt:
            raise RuntimeError("telnet username prompt was not received")
        sock.sendall(username.encode("ascii") + b"\r")
        prompt = recv_until(sock, b"Password?:")
        output.extend(prompt)
        if b"Password?:" not in prompt:
            raise RuntimeError("telnet password prompt was not received")
        sock.sendall(password.encode("ascii") + b"\r")
        return sock, bytes(output)
    except Exception:
        sock.close()
        raise


def run_telnet_probe(host: str, port: int) -> bytes:
    sock, output = begin_telnet_login(
        host, port, GM_COMMAND_LOG_ACCOUNT, GM_COMMAND_LOG_PASSWORD
    )
    with sock:
        welcome = recv_until(sock, b"console")
        output += welcome
        sock.sendall(b"?\r")
        command_list = recv_until(sock, b"Available Commands:")
        sock.sendall(b"B GM_CONSOLE_INPUT_MARKER\r")
        broadcast = recv_until(sock, b"GM_CONSOLE_INPUT_MARKER")
        return output + command_list + broadcast


def run_negative_telnet_probes(host: str, port: int) -> list[str]:
    failures: list[str] = []

    try:
        sock, output = begin_telnet_login(
            host, port, GM_COMMAND_LOG_ACCOUNT, "wrong-gm-command-password"
        )
        with sock:
            tail, closed = recv_until_close(sock)
        output += tail
        if not closed:
            failures.append("wrong-password console connection stayed open")
        if b"Welcome to the Sphere99 console" in output:
            failures.append("wrong-password console login was accepted")
        if b"Bad password for this account." not in output:
            failures.append("wrong-password refusal was not sent to the client")
    except Exception as error:  # noqa: BLE001 - report one bounded row
        failures.append(f"wrong-password probe failed: {error}")

    try:
        sock, output = begin_telnet_login(
            host,
            port,
            GM_COMMAND_LOG_PLAYER_ACCOUNT,
            GM_COMMAND_LOG_PLAYER_PASSWORD,
        )
        with sock:
            tail, closed = recv_until_close(sock)
        output += tail
        if not closed:
            failures.append("non-admin console connection stayed open")
        if b"Welcome to the Sphere99 console" in output:
            failures.append("non-admin console login was accepted")
        if b"Sorry you don't have telnet permission" not in output:
            failures.append("non-admin refusal was not sent to the client")
    except Exception as error:  # noqa: BLE001 - report one bounded row
        failures.append(f"non-admin probe failed: {error}")

    try:
        sock, output = begin_telnet_login(
            host, port, GM_COMMAND_LOG_ACCOUNT, GM_COMMAND_LOG_PASSWORD
        )
        with sock:
            ready = recv_until(sock, b"console")
            if b"console" not in ready:
                failures.append("long-line probe did not authenticate the admin")
            sock.sendall(b"A" * (CONSOLE_INPUT_LIMIT + 512) + b"\r?\r")
            command_list = recv_until(sock, b"Available Commands:")
            output += ready + command_list
        if b"Available Commands:" not in output:
            failures.append("over-long console input broke the session")
    except Exception as error:  # noqa: BLE001 - report one bounded row
        failures.append(f"over-long input probe failed: {error}")

    return failures


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

            try:
                telnet_output = run_telnet_probe(args.host, args.port)
            except Exception as error:  # noqa: BLE001 - retain independent rows
                failures.append(f"positive telnet probe failed: {error}")

            failures.extend(run_negative_telnet_probes(args.host, args.port))

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
    total = len(GM_COMMAND_LOG_MARKERS) + 6
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
