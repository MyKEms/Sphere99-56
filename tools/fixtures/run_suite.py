#!/usr/bin/env python3
"""Run the headless protocol suite against a disposable fixture."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional


SANITIZER_OUTPUT_RE = re.compile(
    r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|"
    r"ThreadSanitizer|MemorySanitizer|runtime error:|"
    r"\b(?:ASan|UBSan|LSan|TSan|MSan)\b",
    re.IGNORECASE,
)


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


def stop_server(process: subprocess.Popen[bytes]) -> int:
    if process.poll() is not None:
        return process.wait()

    process.terminate()
    try:
        return process.wait(timeout=10)
    except subprocess.TimeoutExpired:
        process.kill()
        try:
            return process.wait(timeout=5)
        except subprocess.TimeoutExpired as error:
            raise RuntimeError("server did not exit after SIGKILL") from error


def shutdown_failures(returncode: Optional[int], log_contents: str) -> list[str]:
    failures = []
    if returncode is None:
        failures.append("server exit status was not captured")
    elif returncode != 0:
        failures.append(f"server exited with status {returncode} after fixture shutdown")

    diagnostics = [
        line for line in log_contents.splitlines() if SANITIZER_OUTPUT_RE.search(line)
    ]
    if diagnostics:
        failures.append(f"server log contains {len(diagnostics)} sanitizer diagnostic line(s)")
        failures.extend(f"  {line}" for line in diagnostics[:20])
        if len(diagnostics) > 20:
            failures.append(f"  ... {len(diagnostics) - 20} more matching line(s)")
    return failures


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
    runner_error = None
    shutdown_error = None
    suite_returncode = None
    server_returncode = None
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open fixture server log: {error}", file=sys.stderr)
        return 1

    with log_file:
        try:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.PIPE,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
        except OSError as error:
            runner_error = f"unable to start server: {error}"
        else:
            try:
                wait_for_port(args.host, args.port, args.startup_timeout)
                if process.poll() is not None:
                    raise RuntimeError(
                        f"server exited during startup with {process.returncode}"
                    )
                command = [
                    sys.executable,
                    str(suite),
                    args.host,
                    str(args.port),
                    str(game_port),
                ]
                result = subprocess.run(command, cwd=repo, check=False)
                suite_returncode = result.returncode
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
                    suite_returncode = result.returncode
            except (OSError, RuntimeError) as error:
                runner_error = str(error)
            finally:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    shutdown_error = str(error)

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        shutdown_error = shutdown_error or f"unable to inspect server log: {error}"

    failures = []
    if runner_error:
        failures.append(f"fixture run failed: {runner_error}")
    if suite_returncode is not None and suite_returncode != 0:
        failures.append(f"protocol or lifetime suite exited with status {suite_returncode}")
    if shutdown_error:
        failures.append(f"server shutdown check failed: {shutdown_error}")
    failures.extend(shutdown_failures(server_returncode, log_contents))

    if failures:
        print("synthetic fixture run failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- synthetic fixture server log (tail) ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
