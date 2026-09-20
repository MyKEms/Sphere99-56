#!/usr/bin/env python3
"""Probe opt-in script section execution coverage with synthetic scripts."""

from __future__ import annotations

import argparse
import json
import os
import socket
import subprocess
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server


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


def coverage_report_failures(report: object) -> list[str]:
    """Require exactly three of the five synthetic sections to execute once."""

    if not isinstance(report, dict):
        return ["script execution coverage report root must be an object"]
    entries = report.get("entries")
    if not isinstance(entries, list):
        return ["script execution coverage report has no entries array"]

    expected = {
        ("FUNCTION", "f_coverage_alpha", "function", "f_coverage_alpha", 0): 1,
        ("FUNCTION", "f_coverage_beta", "function", "f_coverage_beta", 0): 1,
        ("FUNCTION", "f_coverage_never", "function", "f_coverage_never", 0): 0,
        ("CHARDEF", "c_MAN", "trigger", "Create", 0): 1,
        ("CHARDEF", "c_MAN", "trigger", "CoverageNever", 0): 0,
    }
    observed: dict[tuple[object, ...], int] = {}
    failures = []
    for entry in entries:
        if not isinstance(entry, dict):
            failures.append(f"invalid script execution coverage entry: {entry!r}")
            continue
        key = (
            entry.get("resource_type"),
            entry.get("resource_name"),
            entry.get("section_kind"),
            entry.get("section_name"),
            entry.get("ordinal"),
        )
        count = entry.get("count")
        if key in observed:
            failures.append(f"duplicate script execution coverage entry: {key!r}")
        if type(count) is not int or count < 0:
            failures.append(f"invalid coverage count for {key!r}: {count!r}")
            continue
        observed[key] = count
        for field in ("resource_index", "resource_page"):
            if type(entry.get(field)) is not int:
                failures.append(f"invalid {field} for script coverage entry {key!r}")

    if set(observed) != set(expected):
        failures.append(
            "coverage sections differed from the five synthetic sections: "
            f"missing={sorted(set(expected) - set(observed), key=repr)!r}, "
            f"unexpected={sorted(set(observed) - set(expected), key=repr)!r}"
        )
    for key in sorted(set(observed) & set(expected), key=repr):
        if observed[key] != expected[key]:
            failures.append(
                f"coverage count for {key!r} was {observed[key]}; expected {expected[key]}"
            )

    if type(report.get("distinct")) is not int or report.get("distinct") != len(entries):
        failures.append(
            f"coverage distinct count was {report.get('distinct')!r}; expected {len(entries)}"
        )
    executed = sum(1 for count in observed.values() if count > 0)
    if type(report.get("executed")) is not int or report.get("executed") != executed:
        failures.append(
            f"coverage executed count was {report.get('executed')!r}; expected {executed}"
        )
    total = sum(observed.values())
    if type(report.get("total")) is not int or report.get("total") != total:
        failures.append(
            f"coverage total was {report.get('total')!r}; expected {total}"
        )
    overflow = report.get("overflow")
    if type(overflow) is not int or overflow != 0:
        failures.append(f"coverage overflow was {overflow!r}; expected 0")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2793)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    parser.add_argument(
        "--expect-disabled",
        action="store_true",
        help="verify no report is emitted when SCRIPTEXECUTIONREPORT is absent",
    )
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    report_path = fixture / "logs" / "script-execution-coverage.json"
    log_path = fixture / "server.log"
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")
    if not (fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {fixture / 'sphere.ini'}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    runner_error = None
    server_returncode = None
    log_contents = ""
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open coverage probe server log: {error}", file=sys.stderr)
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
                sock, _ = game_connect(
                    args.host,
                    args.port,
                    f"coverage_{os.getpid()}",
                    "coverage-password",
                    game_port=args.port + 1000,
                )
                if sock is None:
                    raise RuntimeError("coverage probe did not reach the character list")
                try:
                    sock.sendall(
                        make_char_create(
                            name="CoverageProbe",
                            sex=0,
                            start_loc=1,
                            skill1=25,
                            val1=40,
                            skill2=26,
                            val2=40,
                            skill3=1,
                            val3=20,
                        )
                    )
                    response = decode_game_response(
                        recv_until_game_start(sock, timeout=10.0)
                    )
                    if find_start_packet(response) is None:
                        raise RuntimeError("coverage probe character did not enter the game")
                finally:
                    sock.close()

                if not args.expect_disabled:
                    if process.stdin is None:
                        raise RuntimeError("server console input pipe was not opened")
                    process.stdin.write(b"SERV.SCRIPTCOVERAGEREPORT\n")
                    process.stdin.flush()
                    deadline = time.monotonic() + 5.0
                    while not report_path.is_file() and time.monotonic() < deadline:
                        time.sleep(0.05)
                    if not report_path.is_file():
                        raise RuntimeError(
                            "SERV.SCRIPTCOVERAGEREPORT did not write the report before shutdown"
                        )
            except (OSError, RuntimeError, subprocess.SubprocessError) as error:
                runner_error = str(error)
            finally:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    runner_error = runner_error or f"server shutdown failed: {error}"

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        runner_error = runner_error or f"unable to inspect server log: {error}"

    failures = []
    if runner_error:
        failures.append(f"script execution coverage probe failed: {runner_error}")
    failures.extend(shutdown_failures(server_returncode, log_contents))
    if args.expect_disabled:
        if report_path.exists():
            failures.append("coverage was disabled but a report file was written")
    elif report_path.is_file():
        try:
            report = json.loads(report_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError) as error:
            failures.append(f"coverage report is unreadable or invalid JSON: {error}")
        else:
            failures.extend(coverage_report_failures(report))
    else:
        failures.append("script execution coverage report is missing")

    if failures:
        print("script execution coverage probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- coverage probe server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1

    if args.expect_disabled:
        print("script execution coverage default-off probe passed")
    else:
        print("script execution coverage probe passed: 3/5 sections executed once")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
