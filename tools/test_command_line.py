#!/usr/bin/env python3
"""Verify command-line help and unknown switches never start the server."""

from __future__ import annotations

import argparse
import shutil
import socket
import subprocess
import sys
import tempfile
from pathlib import Path


def _port_is_listening(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=0.2):
            return True
    except OSError:
        return False


def _run_probe(binary: Path, argument: str, port: int) -> list[str]:
    failures: list[str] = []
    fixture = Path(tempfile.mkdtemp(prefix="sphere-command-line-"))
    try:
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).parent / "fixtures" / "make_fixture.py"),
                str(fixture),
            ],
            check=True,
            stdout=subprocess.DEVNULL,
        )
        process = subprocess.Popen(
            [str(binary), argument, f"-P{port}"],
            cwd=fixture,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
        )
        try:
            output, _ = process.communicate(timeout=8)
        except subprocess.TimeoutExpired:
            process.kill()
            output, _ = process.communicate(timeout=3)
            failures.append(f"{argument} left the server running (output: {output[-200:]!r})")
            return failures

        logs = "\n".join(
            path.read_text(encoding="utf-8", errors="replace")
            for path in fixture.glob("*.log")
        )
        combined = f"{output}\n{logs}"
        if process.returncode == 0:
            failures.append(f"{argument} exited successfully instead of rejecting the option")
        if "Command Line Switches:" not in combined:
            failures.append(f"{argument} did not print the command-line usage")
        if _port_is_listening("127.0.0.1", port):
            failures.append(f"{argument} left port {port} listening after exit")
    finally:
        shutil.rmtree(fixture, ignore_errors=True)
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--base-port", type=int, default=31930)
    args = parser.parse_args()
    binary = args.binary.resolve()
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    failures: list[str] = []
    for offset, argument in enumerate(("--help", "-h", "--unknown-option")):
        failures.extend(_run_probe(binary, argument, args.base_port + offset))
    if failures:
        print("command-line startup probe failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    print("command-line startup probe passed: help and unknown options exit without binding")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
