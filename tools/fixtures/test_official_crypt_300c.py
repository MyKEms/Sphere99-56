#!/usr/bin/env python3
"""Replay the 3.0.0c login-crypt trial against the synthetic server."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, wait_for_port
from test_official_crypt import GAME_LOGIN_CAPTURE, check_game_response


LOGIN_300C_CAPTURE = bytes.fromhex(
    """
    ac1500042e5c8ef94ebdfe5402b77d81067c41df90b7246d49db12f684bda1af
    28eb0a0e27ade41a946c8a70fe007fc01f70475c51d71475c51d71c79c31e70c
    f983
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

    ini_path = fixture / "sphere.ini"
    ini_path.write_text(
        ini_path.read_text(encoding="utf-8").replace("DEBUGLEVEL=0", "DEBUGLEVEL=3"),
        encoding="utf-8",
    )

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
                sock.sendall(LOGIN_300C_CAPTURE)
                response = _read_response(sock, process)
            if not response or response[0] != 0xA8:
                failures.append(
                    "3.0.0c login crypt trial did not return the server list "
                    f"(wire_first={response[:1].hex() or 'none'})"
                )
            with socket.create_connection((args.host, args.port), timeout=5.0) as sock:
                sock.sendall(GAME_LOGIN_CAPTURE)
                game_response = _read_response(sock, process)
            for failure in check_game_response(game_response):
                failures.append(f"3.0.0c game trial: {failure}")
        except (OSError, RuntimeError) as error:
            failures.append(str(error))
        finally:
            returncode = stop_server(process)

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    if not re.search(r"xProcessClientSetup result lErr=255 connType=3 cryptVer=0x300000", log_contents):
        failures.append("3.0.0c login crypt trial did not select 0x300000")

    if failures:
        print("3.0.0c crypt trial fixture failed:")
        for failure in failures:
            print(f"- {failure}")
        print("\n--- server log (tail) ---")
        print("\n".join(log_contents.splitlines()[-80:]))
        return 1

    print(
        "3.0.0c crypt trial fixture passed: login selected 0x300000 and "
        f"returned the server list and stock game framing ({len(response)} bytes)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
