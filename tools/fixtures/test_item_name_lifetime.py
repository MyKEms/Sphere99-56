#!/usr/bin/env python3
"""Load individually named items and check the names read from them.

The fixture (make_fixture.py --world-load-counts --named-item-names) saves a
named IT_MULTI item, whose region name is built from the item name while the
save loads, and a named item whose saved timer expires on the first sector
tick and logs the item name from an explicit handler.  Run it under ASan to
catch a name pointer that outlives its storage; the logged timer name is also
checked byte for byte.
"""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

from make_fixture import NAMED_TIMER_ITEM_NAME
from run_suite import shutdown_failures, stop_server


EXPECTED_COUNT_LINE = (
    "world load: created_items=2 created_chars=1 read_items=2 read_chars=1 "
    "allocated_items=2 allocated_chars=1"
)
TIMER_LINE_RE = re.compile(r"SPHERE_NAMED_TIMER_TICK (.*)$")


def read_log(path: Path) -> str:
    try:
        return path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return ""


def port_is_open(host: str, port: int) -> bool:
    try:
        with socket.create_connection((host, port), timeout=1.0):
            return True
    except OSError:
        return False


def wait_until(process: subprocess.Popen[bytes], timeout: float, ready) -> bool:
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if ready():
            return True
        if process.poll() is not None:
            return ready()
        time.sleep(0.2)
    return ready()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2724)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    parser.add_argument(
        "--timer-timeout",
        type=float,
        default=60.0,
        help="seconds to wait for the saved item timer to expire after startup",
    )
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    log_path = fixture / "server.log"
    if not (fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {fixture / 'sphere.ini'}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    failures: list[str] = []
    server_returncode = None
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open server log: {error}", file=sys.stderr)
        return 1

    with log_file:
        try:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
        except OSError as error:
            failures.append(f"unable to start server: {error}")
        else:
            try:
                if not wait_until(
                    process,
                    args.startup_timeout,
                    lambda: port_is_open(args.host, args.port),
                ):
                    failures.append(
                        "server did not listen after loading the named items "
                        f"(exit status {process.poll()})"
                    )
                else:
                    # The server console is buffered while it is running, so
                    # inspect the marker after the bounded wait and shutdown.
                    wait_until(
                        process,
                        args.timer_timeout,
                        lambda: TIMER_LINE_RE.search(read_log(log_path)) is not None,
                    )
            finally:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    failures.append(f"server shutdown failed: {error}")

    log_contents = read_log(log_path)
    failures.extend(shutdown_failures(server_returncode, log_contents))

    if EXPECTED_COUNT_LINE not in log_contents:
        failures.append(f"missing world-load line {EXPECTED_COUNT_LINE!r}")

    timer_names = TIMER_LINE_RE.findall(log_contents)
    if timer_names != [NAMED_TIMER_ITEM_NAME]:
        failures.append(
            f"timer handler names were {timer_names!r}; "
            f"expected {[NAMED_TIMER_ITEM_NAME]!r}"
        )

    unexpected_errors = [
        line
        for line in log_contents.splitlines()
        if (line.startswith("[ERROR]") or line.startswith("[CRITICAL]"))
        and not TIMER_LINE_RE.search(line)
    ]
    if unexpected_errors:
        failures.append(f"unexpected server errors: {unexpected_errors[:10]!r}")

    if failures:
        print("item name lifetime probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1

    print(
        "item name lifetime probe passed: named multi loaded, "
        f"timer handler logged {NAMED_TIMER_ITEM_NAME!r}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
