#!/usr/bin/env python3
"""Check that malformed BOOK and ITEMDEF resource IDs are bounded at load."""

from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

from run_suite import shutdown_failures, stop_server, tail, wait_for_port


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2740)
    parser.add_argument("--startup-timeout", type=float, default=30.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    script_path = fixture / "scripts" / "spheretables.scp"
    try:
        script_text = script_path.read_text(encoding="ascii")
    except OSError as error:
        parser.error(f"unable to read fixture script: {error}")
    required_markers = (
        "[BOOK SYNTHETIC_BOOK]",
        "[BOOK SYNTHETIC_BOOK 127]",
        "[ITEMDEF 0FFFFFFF0]",
    )
    for marker in required_markers:
        if marker not in script_text:
            parser.error(f"fixture is missing required marker {marker!r}")

    log_path = fixture / "server.log"
    process = None
    server_returncode = None
    failures: list[str] = []
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.PIPE,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            try:
                wait_for_port(args.host, args.port, args.startup_timeout)
                if process.poll() is not None:
                    failures.append(
                        f"server exited during startup with {process.returncode}"
                    )
            except (OSError, RuntimeError) as error:
                failures.append(f"server did not start: {error}")
            finally:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    failures.append(f"server shutdown check failed: {error}")
    except OSError as error:
        failures.append(f"unable to run fixture server: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read fixture server log: {error}")

    failures.extend(shutdown_failures(server_returncode, log_contents))
    if "Exception loading section" in log_contents:
        failures.append("resource load still reports an exception")
    if "LOAD_EXCEPTION" in log_contents:
        failures.append("resource load emitted diagnostic instrumentation")
    unexpected_errors = [
        line
        for line in log_contents.splitlines()
        if line.startswith("[ERROR]")
        or line.startswith("[CRITICAL]")
        or line.startswith("[FATAL]")
    ]
    if unexpected_errors:
        failures.append(
            "resource fixture emitted unexpected error diagnostics: "
            + repr(unexpected_errors[:8])
        )
    unexpected_err_lines = [
        line
        for line in log_contents.splitlines()
        if line.startswith("[ERR]") and "DebugLevel set to" not in line
    ]
    if unexpected_err_lines:
        failures.append(
            "resource fixture emitted unexpected [ERR] diagnostics: "
            + repr(unexpected_err_lines[:8])
        )

    if failures:
        print("BOOK/ITEMDEF resource-load fixture failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- fixture server log (tail) ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1

    print("BOOK/ITEMDEF resource-load fixture passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
