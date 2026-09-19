#!/usr/bin/env python3
"""Run the headless protocol suite against a disposable fixture."""

from __future__ import annotations

import argparse
import json
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


def newbie_load_failures(
    log_contents: str, expected_sections: tuple[str, ...] = ()
) -> list[str]:
    invalid_sections = [
        line for line in log_contents.splitlines()
        if "Invalid NEWBIE block index" in line
    ]
    invalid_names = []
    unparsed_lines = []
    for line in invalid_sections:
        match = re.search(r"Invalid NEWBIE block index '([^']+)'", line)
        if match:
            invalid_names.append(match.group(1))
        else:
            unparsed_lines.append(line)
    if invalid_names == list(expected_sections) and not unparsed_lines:
        return []
    failures = [
        f"server log contains {len(invalid_sections)} invalid NEWBIE block index "
        f"error(s) with names {invalid_names!r}; "
        f"expected {list(expected_sections)!r}"
    ]
    failures.extend(f"  {line}" for line in invalid_sections[:20])
    if len(invalid_sections) > 20:
        failures.append(f"  ... {len(invalid_sections) - 20} more error(s)")
    return failures


def unknown_keyword_failures(report: dict, allowlist: dict) -> list[str]:
    """Require a valid report containing only explicitly allowlisted hits."""

    if not isinstance(report, dict):
        return ["unknown-keyword report root must be an object"]
    if not isinstance(allowlist, dict):
        return ["unknown-keyword allowlist root must be an object"]

    failures = []
    report_entries = report.get("entries")
    allowed_entries = allowlist.get("entries")
    if not isinstance(report_entries, list):
        return ["unknown-keyword report has no entries array"]
    if not isinstance(allowed_entries, list):
        return ["unknown-keyword allowlist has no entries array"]

    observed = {}
    for entry in report_entries:
        if not isinstance(entry, dict):
            failures.append(f"invalid unknown-keyword report entry: {entry!r}")
            continue
        key = (entry.get("kind"), entry.get("keyword"))
        if not isinstance(key[0], str) or not isinstance(key[1], str):
            failures.append(f"invalid key in unknown-keyword report: {key!r}")
            continue
        if key in observed:
            failures.append(f"duplicate unknown-keyword report key: {key!r}")
        count = entry.get("count")
        if type(count) is not int or count < 1:
            failures.append(f"invalid count for unknown-keyword report key {key!r}: {count!r}")
        observed[key] = entry

    allowed = {}
    for entry in allowed_entries:
        if not isinstance(entry, dict):
            failures.append(f"invalid unknown-keyword allowlist entry: {entry!r}")
            continue
        key = (entry.get("kind"), entry.get("keyword"))
        if not isinstance(key[0], str) or not isinstance(key[1], str):
            failures.append(f"invalid key in unknown-keyword allowlist: {key!r}")
            continue
        if not isinstance(entry.get("optional", False), bool):
            failures.append(f"invalid optional flag for allowlisted key {key!r}")
        count = entry.get("count")
        count_range = entry.get("count_range")
        if count is not None:
            if type(count) is not int or count < 1:
                failures.append(f"invalid allowlisted count for {key!r}: {count!r}")
        elif (
            not isinstance(count_range, list)
            or len(count_range) != 2
            or type(count_range[0]) is not int
            or type(count_range[1]) is not int
            or count_range[0] < 1
            or count_range[1] < count_range[0]
        ):
            failures.append(
                f"invalid allowlisted count range for {key!r}: {count_range!r}"
            )
        if count is None and count_range is None:
            failures.append(
                f"allowlisted key {key!r} requires count or count_range"
            )
        elif count is not None and count_range is not None:
            failures.append(
                f"allowlisted key {key!r} cannot set both count and count_range"
            )
        if key in allowed:
            failures.append(f"duplicate unknown-keyword allowlist key: {key!r}")
        allowed[key] = entry

    unexpected = sorted(set(observed) - set(allowed), key=repr)
    missing = sorted(
        (
            key
            for key in set(allowed) - set(observed)
            if not allowed[key].get("optional", False)
        ),
        key=repr,
    )
    if unexpected:
        failures.append(f"unexpected unknown-keyword keys: {unexpected!r}")
    if missing:
        failures.append(f"allowlisted unknown-keyword keys were not observed: {missing!r}")
    for key in sorted(set(observed) & set(allowed), key=repr):
        actual = observed[key].get("count")
        expected = allowed[key].get("count")
        if expected is not None and actual != expected:
            failures.append(
                f"unknown-keyword count for {key!r} was {actual!r}; expected {expected!r}"
            )
        elif expected is None:
            count_range = allowed[key].get("count_range")
            if (
                isinstance(count_range, list)
                and len(count_range) == 2
                and type(count_range[0]) is int
                and type(count_range[1]) is int
                and type(actual) is int
                and (actual < count_range[0] or actual > count_range[1])
            ):
                failures.append(
                    f"unknown-keyword count for {key!r} was {actual!r}; "
                    f"expected count {count_range[0]}..{count_range[1]}"
                )

    overflow = report.get("overflow")
    if type(overflow) is not int or overflow < 0:
        failures.append(f"invalid unknown-keyword overflow counter: {overflow!r}")
    elif overflow:
        failures.append(f"unknown-keyword report overflowed by {overflow} hit(s)")

    distinct = report.get("distinct")
    if type(distinct) is not int or distinct < 0:
        failures.append(f"invalid unknown-keyword distinct counter: {distinct!r}")
    elif distinct != len(report_entries):
        failures.append(
            f"unknown-keyword distinct count was {distinct!r}; "
            f"expected {len(report_entries)}"
        )
    total = report.get("total")
    entry_total = sum(
        entry.get("count", 0)
        for entry in report_entries
        if isinstance(entry, dict) and type(entry.get("count")) is int
    )
    expected_total = entry_total + (
        overflow if type(overflow) is int and overflow >= 0 else 0
    )
    if type(total) is not int or total < 0:
        failures.append(f"invalid unknown-keyword total counter: {total!r}")
    elif total != expected_total:
        failures.append(
            f"unknown-keyword total was {total!r}; "
            f"entries and overflow add to {expected_total}"
        )
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
        "--expect-invalid-newbie",
        action="append",
        default=[],
        metavar="SECTION",
        help="expect this exact invalid NEWBIE section name in the server log",
    )
    parser.add_argument(
        "--unknown-keyword-allowlist",
        type=Path,
        metavar="JSON",
        help="fail on any runtime unresolved keyword outside this checked-in allowlist",
    )
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
    failures.extend(newbie_load_failures(log_contents, tuple(args.expect_invalid_newbie)))

    if args.unknown_keyword_allowlist:
        report_path = fixture / "logs" / "unknown-keywords.json"
        try:
            report = json.loads(report_path.read_text(encoding="utf-8"))
        except OSError as error:
            failures.append(f"unknown-keyword report is missing or unreadable: {error}")
        except json.JSONDecodeError as error:
            failures.append(f"unknown-keyword report is not valid JSON: {error}")
        else:
            try:
                allowlist = json.loads(
                    args.unknown_keyword_allowlist.resolve().read_text(encoding="utf-8")
                )
            except OSError as error:
                failures.append(f"unknown-keyword allowlist is missing or unreadable: {error}")
            except json.JSONDecodeError as error:
                failures.append(f"unknown-keyword allowlist is not valid JSON: {error}")
            else:
                failures.extend(unknown_keyword_failures(report, allowlist))

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
