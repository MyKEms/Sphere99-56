#!/usr/bin/env python3
"""Verify that a non-TTY stdin line reaches the existing console command path."""

from __future__ import annotations

import argparse
import subprocess
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, tail, wait_for_port


SAVE_START = "World save started: SaveCount="
SAVE_END = "World save ended: SaveCount="


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2890)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--save-timeout", type=float, default=20.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    process: subprocess.Popen[bytes] | None = None
    returncode: int | None = None
    try:
        with (fixture / "server.log").open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.PIPE,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port(args.host, args.port, args.startup_timeout)
            if process.stdin is None:
                raise RuntimeError("server stdin pipe was not created")
            process.stdin.write(b"#\n")
            process.stdin.flush()

            deadline = time.monotonic() + args.save_timeout
            while time.monotonic() < deadline:
                contents = (fixture / "server.log").read_text(
                    encoding="utf-8", errors="replace"
                )
                if SAVE_END in contents:
                    break
                time.sleep(0.1)
            else:
                failures.append(
                    "non-TTY '#' command did not complete a world save within "
                    f"{args.save_timeout:.1f}s"
                )
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        contents = (fixture / "server.log").read_text(
            encoding="utf-8", errors="replace"
        )
    except OSError as error:
        contents = ""
        failures.append(f"unable to read server log: {error}")
    failures.extend(shutdown_failures(returncode, contents))
    if SAVE_START not in contents:
        failures.append("world save start marker is missing")
    if SAVE_END not in contents:
        failures.append("world save end marker is missing")

    if failures:
        print("console stdin probe failed", flush=True)
        for failure in failures:
            print(f"- {failure}")
        print("\n--- server log tail ---")
        print(tail(fixture / "server.log"))
        return 1
    print("console stdin probe passed: non-TTY '#' completed a world save")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
