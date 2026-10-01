#!/usr/bin/env python3
"""Replay a captured official-client game-login stream to the fixture server."""

from __future__ import annotations

import argparse
import socket
import subprocess
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, wait_for_port


# The first four bytes are the game-connection seed.  The remaining bytes are
# the captured 3.0.6m login request.  Keeping this stream as hex makes the
# regression reproducible without distributing a client or a private capture.
GAME_LOGIN_CAPTURE = bytes.fromhex(
    """
    7f0000012262d95047b301fb09b7403b
    8c3c0a8efca3cf8dcf23d65306a1d860
    ddc0dc34b6a8ee7f6725df5f6bb38b0f
    99557a853badb96b36974a7d5b2eed2e
    23a312cd0d
    """
)


def _read_response(sock: socket.socket, process: subprocess.Popen[bytes]) -> bytes:
    data = bytearray()
    deadline = time.monotonic() + 5.0
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(4096)
        except socket.timeout:
            if process.poll() is not None:
                break
            continue
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        data.extend(chunk)
        if data and data[0] == 0x81:
            break
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, required=True)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
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
            with socket.create_connection((args.host, args.port), timeout=5.0) as sock:
                sock.sendall(GAME_LOGIN_CAPTURE)
                response = _read_response(sock, process)
        except (OSError, RuntimeError) as error:
            failures.append(str(error))
        finally:
            returncode = stop_server(process)

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    if not response or response[0] != 0x81:
        first = response[:1].hex() or "none"
        failures.append(f"captured game login produced no character-list packet (first={first})")

    if failures:
        print("official crypt negotiation fixture failed:")
        for failure in failures:
            print(f"- {failure}")
        print("\n--- server log (tail) ---")
        print("\n".join(log_contents.splitlines()[-80:]))
        return 1

    print(
        "official crypt negotiation fixture passed: captured game login returned "
        f"character list ({len(response)} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
