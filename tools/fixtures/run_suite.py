#!/usr/bin/env python3
"""Run the headless protocol suite against a disposable fixture."""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import time
from pathlib import Path


def wait_for_port(host: str, port: int, timeout: float) -> None:
    deadline = time.monotonic() + timeout
    last_error = "not attempted"
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=1.0):
                return
        except OSError as error:
            last_error = str(error)
            time.sleep(0.2)
    raise RuntimeError(f"server did not listen on {host}:{port}: {last_error}")


def tail(path: Path, lines: int = 80) -> str:
    try:
        content = path.read_text(encoding="utf-8", errors="replace").splitlines()
    except OSError as error:
        return f"unable to read server log: {error}"
    return "\n".join(content[-lines:])


def stop_server(process: subprocess.Popen[bytes]) -> None:
    if process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--repo", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2593)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument(
        "--lifetime-soak",
        type=int,
        default=0,
        metavar="CYCLES",
        help="run the bounded client lifetime soak after the protocol suite",
    )
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    repo = args.repo.resolve()
    suite = repo / "tools" / "test_suite.py"
    log_path = fixture / "server.log"
    game_port = args.port + 1000

    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")
    if not suite.is_file():
        parser.error(f"test suite does not exist: {suite}")

    print(
        f"starting synthetic fixture: login {args.host}:{args.port}, "
        f"game {args.host}:{game_port}"
    )
    with log_path.open("wb") as log:
        process = subprocess.Popen(
            [str(binary), f"-P{args.port}"],
            cwd=fixture,
            stdin=subprocess.PIPE,
            stdout=log,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port(args.host, args.port, args.startup_timeout)
            if process.poll() is not None:
                raise RuntimeError(f"server exited during startup with {process.returncode}")
            command = [
                sys.executable,
                str(suite),
                args.host,
                str(args.port),
                str(game_port),
            ]
            result = subprocess.run(command, cwd=repo, check=False)
            if result.returncode == 0 and args.lifetime_soak:
                soak = repo / "tools" / "fixtures" / "lifetime_soak.py"
                result = subprocess.run(
                    [
                        sys.executable,
                        str(soak),
                        args.host,
                        str(args.port),
                        str(game_port),
                        str(args.lifetime_soak),
                    ],
                    cwd=repo,
                    check=False,
                )
            if result.returncode:
                print("\n--- synthetic fixture server log (tail) ---", file=sys.stderr)
                print(tail(log_path), file=sys.stderr)
            return result.returncode
        except (OSError, RuntimeError) as error:
            print(f"fixture run failed: {error}", file=sys.stderr)
            print("\n--- synthetic fixture server log (tail) ---", file=sys.stderr)
            print(tail(log_path), file=sys.stderr)
            return 1
        finally:
            stop_server(process)


if __name__ == "__main__":
    raise SystemExit(main())
