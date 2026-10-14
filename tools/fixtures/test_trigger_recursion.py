#!/usr/bin/env python3
"""Verify script-level damage_noresponse avoids trigger recursion."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
from pathlib import Path

from modes.compat_writer import (
    TRIGGER_RECURSION_ACCOUNT,
    TRIGGER_RECURSION_MARKER,
    TRIGGER_RECURSION_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


def system_messages(data: bytes) -> list[str]:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_packets import split_packet_stream

    messages: list[str] = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0]
        messages.append(text.decode("ascii", errors="replace"))
    return messages


def enter_world(host: str, port: int) -> bytes:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        host,
        port,
        TRIGGER_RECURSION_ACCOUNT,
        TRIGGER_RECURSION_PASSWORD,
        game_port=port + 1000,
    )
    if sock is None:
        raise RuntimeError("probe account did not reach its character list")
    try:
        sock.sendall(make_char_create(name=TRIGGER_RECURSION_ACCOUNT, start_loc=1))
        response = decode_game_response(recv_until_game_start(sock, timeout=30))
    finally:
        sock.close()
    if find_start_packet(response) is None:
        raise RuntimeError("probe character did not receive a game-entry packet")
    return response


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=6400)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    log_path = fixture / "server.log"
    failures: list[str] = []
    response = b""
    returncode: int | None = None
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            response = enter_world(args.host, args.port)
        except (OSError, RuntimeError, socket.timeout) as error:
            failures.append(str(error))
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    messages = system_messages(response)
    if f"{TRIGGER_RECURSION_MARKER}_RETURNED" not in messages:
        failures.append("the trigger chain did not return to the login script")
    if f"{TRIGGER_RECURSION_MARKER}_FUNCTION" not in messages:
        failures.append("damage_noresponse did not dispatch its script function")
    diagnostics = [
        line for line in log_contents.splitlines() if "Trigger Recursion error !" in line
    ]
    if diagnostics:
        failures.append(
            "unexpected trigger recursion diagnostic: " + diagnostics[0]
        )

    total = 4
    passed = max(0, total - len(failures))
    if failures:
        print(f"trigger-recursion probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-100:]), file=sys.stderr)
        return 1
    print(f"trigger-recursion probe passed: {total}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
