#!/usr/bin/env python3
"""Exercise deferred chat-channel destruction through a client disconnect."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path

from run_suite import SANITIZER_OUTPUT_RE, shutdown_failures, stop_server, wait_for_port


CHAT_OPEN = 0x03ED
CHAT_CHANNEL_BAR = 0x03F1


def make_chat_button(name: str) -> bytes:
    encoded = name.encode("utf-16-be")[:60]
    packet = bytearray(64)
    packet[0] = 0xB5
    packet[2:2 + len(encoded)] = encoded
    return bytes(packet)


def make_chat_text(text: str) -> bytes:
    encoded = (text + "\0").encode("utf-16-be")
    length = 7 + len(encoded)
    return b"\xB3" + length.to_bytes(2, "big") + b"enu\0" + encoded


def find_chat_subcommand(raw: bytes, subcommand: int) -> bool:
    for offset in range(len(raw) - 5):
        if raw[offset] != 0xB2:
            continue
        length = int.from_bytes(raw[offset + 1:offset + 3], "big")
        if length >= 9 and offset + length <= len(raw):
            if int.from_bytes(raw[offset + 3:offset + 5], "big") == subcommand:
                return True
    return False


def recv_until_chat(sock: socket.socket, subcommand: int, timeout: float) -> bytes:
    from uo_test_client import decode_game_response

    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.5)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
        if find_chat_subcommand(decode_game_response(bytes(data)), subcommand):
            return bytes(data)
    return bytes(data)


def run_probe(fixture: Path, binary: Path, port: int, startup_timeout: float) -> int:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (
        decode_game_response,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    log_path = fixture / "server.log"
    ini_path = fixture / "sphere.ini"
    ini_path.write_text(
        ini_path.read_text(encoding="ascii").replace("DEBUGLEVEL=0", "DEBUGLEVEL=1"),
        encoding="ascii",
    )
    failures: list[str] = []
    returncode: int | None = None
    client: socket.socket | None = None

    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port("127.0.0.1", port, startup_timeout)
            client, _ = game_connect(
                "127.0.0.1", port, "ChatLifetime", "chat-lifetime-pw", game_port=port + 1000
            )
            if client is None:
                failures.append("chat client did not reach the character list")
            else:
                client.sendall(make_char_create(name="ChatLifetime", start_loc=1))
                start = recv_until_game_start(client, timeout=30.0)
                if not start:
                    failures.append("chat client did not enter the world")
                else:
                    client.sendall(make_chat_button("ChatLifetime"))
                    opened = recv_until_chat(client, CHAT_OPEN, timeout=10.0)
                    if not find_chat_subcommand(decode_game_response(opened), CHAT_OPEN):
                        failures.append("chat window did not open")
                    client.sendall(make_chat_text("cLifetimeRoom"))
                    channel = recv_until_chat(client, CHAT_CHANNEL_BAR, timeout=10.0)
                    if not find_chat_subcommand(
                        decode_game_response(channel), CHAT_CHANNEL_BAR
                    ):
                        failures.append("chat channel was not created and joined")
                    client.close()
                    client = None

            time.sleep(0.2)
            if process.poll() is not None:
                failures.append(f"server exited before probe completion with status {process.returncode}")
        except (OSError, RuntimeError, ValueError) as error:
            failures.append(str(error))
        finally:
            if client is not None:
                try:
                    client.close()
                except OSError:
                    pass
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    if SANITIZER_OUTPUT_RE.search(log_contents):
        failures.append("server log contains sanitizer output")
    callback_count = log_contents.count("CChatChannel quit callback")
    if callback_count != 1:
        failures.append(
            f"chat callback diagnostic occurred {callback_count} times; expected exactly once"
        )
    destruction_count = log_contents.count("CChatChannel destroyed")
    if destruction_count != 1:
        failures.append(
            f"chat destruction diagnostic occurred {destruction_count} times; expected exactly once"
        )
    callback_offset = log_contents.find("CChatChannel quit callback")
    destruction_offset = log_contents.find("CChatChannel destroyed")
    if (
        callback_offset >= 0
        and destruction_offset >= 0
        and destruction_offset < callback_offset
    ):
        failures.append("chat channel was destroyed before the quit callback completed")

    if failures:
        print("chat lifetime probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1

    print("chat lifetime probe passed: callback completed before channel destruction")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2865)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    return run_probe(args.fixture.resolve(), args.binary.resolve(), args.port, args.startup_timeout)


if __name__ == "__main__":
    raise SystemExit(main())
