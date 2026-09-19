#!/usr/bin/env python3
"""Probe unresolved-keyword reporting with synthetic and headless clients."""

from __future__ import annotations

import argparse
import csv
import json
import os
import socket
import struct
import subprocess
import sys
import time
from pathlib import Path
from typing import Any

from run_suite import shutdown_failures, stop_server, wait_for_port


def source_line(script_path: Path, marker: str) -> int:
    matches = [
        line_number
        for line_number, line in enumerate(
            script_path.read_text(encoding="ascii").splitlines(), start=1
        )
        if marker in line
    ]
    if len(matches) != 1:
        raise RuntimeError(
            f"expected one source line containing {marker!r}, found {matches!r}"
        )
    return matches[0]


def probe_failures(
    fixture: Path,
    report_path: Path,
    *,
    rejected: bool = False,
    normalization: bool = False,
    expect_set: bool = False,
    csv_format: bool = False,
) -> list[str]:
    try:
        if csv_format:
            with report_path.open(encoding="utf-8", newline="") as stream:
                reader = csv.DictReader(stream)
                expected_fields = [
                    "kind",
                    "keyword",
                    "count",
                    "first_file",
                    "first_line",
                    "first_object_type",
                ]
                if reader.fieldnames != expected_fields:
                    return [f"unexpected CSV report header: {reader.fieldnames!r}"]
                rows = list(reader)
            entries = []
            summary = {}
            for row in rows:
                if row["kind"] in ("overflow", "total"):
                    summary[row["kind"]] = int(row["count"])
                    continue
                entries.append(
                    {
                        "kind": row["kind"],
                        "keyword": row["keyword"],
                        "count": int(row["count"]),
                        "first_file": row["first_file"],
                        "first_line": int(row["first_line"]),
                        "first_object_type": row["first_object_type"],
                    }
                )
            if set(summary) != {"overflow", "total"}:
                return [f"CSV report has incomplete summary rows: {summary!r}"]
            report: dict[str, Any] = {
                "distinct": len(entries),
                "total": summary["total"],
                "overflow": summary["overflow"],
                "entries": entries,
            }
        else:
            report = json.loads(report_path.read_text(encoding="utf-8"))
    except OSError as error:
        return [f"unknown-keyword report is missing or unreadable: {error}"]
    except json.JSONDecodeError as error:
        return [f"unknown-keyword report is not valid JSON: {error}"]
    except (csv.Error, KeyError, ValueError) as error:
        return [f"unknown-keyword CSV report is malformed: {error}"]

    script_path = fixture / "scripts" / "spheretables.scp"
    expected = (
        {
            ("rejected", "ARG"): "ARG(unknown_report_bad_qty,one,two)",
        }
        if rejected
        else {
            ("method", "UNKNOWN_REPORT_PROBE_COMMAND"): "UNKNOWN_REPORT_PROBE_COMMAND",
            ("get", "UNKNOWN_REPORT_PROBE_PROPERTY"): "SPHERE_UNKNOWN_PROBE_PROPERTY",
            ("function", "UNKNOWN_REPORT_PROBE_FUNCTION"): "SPHERE_UNKNOWN_PROBE_FUNCTION",
            ("trigger", "@UNKNOWNREPORTPROBE"): "TRIGGER @UnknownReportProbe",
        }
    )
    if normalization:
        expected.update(
            {
                ("get", "UNKNOWN_REPORT_DOTTED_TAG.*"): "SPHERE_UNKNOWN_NORMALIZED_DOTTED",
                ("get", "UNKNOWN_REPORT_INDEX_ARGV[]"): "SPHERE_UNKNOWN_NORMALIZED_INDEX",
            }
        )
    if expect_set:
        expected[("set", "UNKNOWN_REPORT_PROBE_SET")] = "UNKNOWN_REPORT_PROBE_SET=1"
    expected_lines = {
        key: source_line(script_path, marker) for key, marker in expected.items()
    }

    entries = report.get("entries")
    if not isinstance(entries, list):
        return ["unknown-keyword report has no entries array"]

    failures = []
    observed: dict[tuple[str, str], dict[str, Any]] = {}
    for entry in entries:
        if not isinstance(entry, dict):
            failures.append(f"invalid report entry: {entry!r}")
            continue
        key = (entry.get("kind"), entry.get("keyword"))
        if key in observed:
            failures.append(f"duplicate report key: {key!r}")
        observed[key] = entry

    if set(observed) != set(expected):
        failures.append(
            f"expected exactly {sorted(expected)!r}, got {sorted(observed)!r}"
        )

    for key, expected_line in expected_lines.items():
        entry = observed.get(key)
        if entry is None:
            continue
        expected_count = 2 if rejected and key == ("rejected", "ARG") else 1
        if entry.get("count") != expected_count:
            failures.append(
                f"{key!r}: expected count {expected_count}, got {entry.get('count')!r}"
            )
        if entry.get("first_file") != "spheretables.scp":
            failures.append(
                f"{key!r}: expected first_file 'spheretables.scp', "
                f"got {entry.get('first_file')!r}"
            )
        if entry.get("first_line") != expected_line:
            failures.append(
                f"{key!r}: expected first_line {expected_line}, "
                f"got {entry.get('first_line')!r}"
            )
        if entry.get("first_object_type") != "CChar":
            failures.append(
                f"{key!r}: expected first_object_type 'CChar', "
                f"got {entry.get('first_object_type')!r}"
            )

    if report.get("distinct") != len(expected):
        failures.append(
            f"expected distinct={len(expected)}, got {report.get('distinct')!r}"
        )
    expected_total = 2 if rejected else len(expected)
    if report.get("total") != expected_total:
        failures.append(
            f"expected total={expected_total}, got {report.get('total')!r}"
        )
    if report.get("overflow") != 0:
        failures.append(f"expected overflow=0, got {report.get('overflow')!r}")
    if ("function", "EVAL") in observed:
        failures.append("known EVAL call was reported as unresolved")
    if any("UNKNOWN_PROBE_KNOWN" in str(key) for key in observed):
        failures.append("known probe call was reported as unresolved")
    return failures


def overflow_probe_failures(report_path: Path) -> list[str]:
    try:
        report: dict[str, Any] = json.loads(report_path.read_text(encoding="utf-8"))
    except OSError as error:
        return [f"unknown-keyword report is missing or unreadable: {error}"]
    except json.JSONDecodeError as error:
        return [f"unknown-keyword report is not valid JSON: {error}"]

    entries = report.get("entries")
    if not isinstance(entries, list):
        return ["unknown-keyword report has no entries array"]
    expected = {
        ("get", f"UNKNOWN_REPORT_OVERFLOW_{index:04d}") for index in range(1024)
    }
    observed = {
        (entry.get("kind"), entry.get("keyword"))
        for entry in entries
        if isinstance(entry, dict)
    }
    failures = []
    if observed != expected:
        failures.append(
            f"expected the first 1,024 overflow-probe keys, got "
            f"{len(observed)} distinct keys"
        )
    if any(
        not isinstance(entry, dict) or entry.get("count") != 1
        for entry in entries
    ):
        failures.append("overflow probe entries must each have count 1")
    if report.get("distinct") != 1024 or len(entries) != 1024:
        failures.append(
            f"expected exactly 1,024 retained keys, got distinct="
            f"{report.get('distinct')!r}, entries={len(entries)}"
        )
    if report.get("total") != 1025:
        failures.append(f"expected total=1025, got {report.get('total')!r}")
    if report.get("overflow") != 1:
        failures.append(f"expected overflow=1, got {report.get('overflow')!r}")
    return failures


def drain_available(sock: socket.socket) -> bytes:
    """Read currently queued game data and return its decoded packet bytes."""

    chunks = bytearray()
    sock.setblocking(False)
    try:
        while True:
            try:
                chunk = sock.recv(65536)
            except BlockingIOError:
                break
            if not chunk:
                raise RuntimeError("headless scenario client disconnected")
            chunks.extend(chunk)
    finally:
        sock.setblocking(True)
        sock.settimeout(10.0)
    if not chunks:
        return b""
    from uo_test_client import decode_game_response  # pylint: disable=import-outside-toplevel

    return decode_game_response(bytes(chunks))


def make_speech_packet(text: str) -> bytes:
    """Build a classic ASCII speech packet for the disposable headless client."""

    text_bytes = text.encode("ascii") + b"\0"
    length = 8 + len(text_bytes)
    return (
        b"\x03"
        + struct.pack(">H", length)
        + b"\x03"
        + struct.pack(">HH", 0x03B2, 3)
        + text_bytes
    )


def run_headless_scenario(
    sock: socket.socket,
    *,
    duration: float,
) -> None:
    """Exercise movement, speech, pack access, and harmless GM verbs."""

    started_at = time.monotonic()
    deadline = started_at + duration

    def send_and_drain(packet: bytes) -> None:
        sock.sendall(packet)
        time.sleep(0.1)
        drain_available(sock)

    sequence = 0

    def walk_steps(count: int) -> None:
        nonlocal sequence
        for _ in range(count):
            sequence += 1
            direction = sequence % 8
            send_and_drain(struct.pack(">BBBI", 0x02, direction, sequence & 0xFF, 0))

    walk_steps(5)
    send_and_drain(make_speech_packet("baseline speech check"))
    for command in ("/OPENPACK", "/WHERE", "/VERSION", "/INFORMATION"):
        send_and_drain(make_speech_packet(command))

    next_activity = time.monotonic() + 10.0
    next_gm_verbs = time.monotonic() + 30.0
    while time.monotonic() < deadline:
        now = time.monotonic()
        if now >= next_activity:
            walk_steps(2)
            send_and_drain(make_speech_packet("baseline activity check"))
            next_activity = time.monotonic() + 10.0
        if now >= next_gm_verbs:
            for command in ("/WHERE", "/VERSION", "/INFORMATION"):
                send_and_drain(make_speech_packet(command))
            next_gm_verbs = time.monotonic() + 30.0
        if time.monotonic() < deadline:
            time.sleep(min(0.1, deadline - time.monotonic()))

    drain_available(sock)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2693)
    parser.add_argument(
        "--startup-timeout",
        type=float,
        default=600.0,
        help="maximum startup wait for script trees that load for several minutes",
    )
    expectations = parser.add_mutually_exclusive_group()
    expectations.add_argument(
        "--expect-disabled",
        action="store_true",
        help="verify unknown keywords produce no report when no path is configured",
    )
    expectations.add_argument(
        "--normalization",
        action="store_true",
        help="expect the four-key probe plus dotted and numeric-index cases",
    )
    expectations.add_argument(
        "--overflow",
        action="store_true",
        help="verify the 1,024-key retention cap and overflow counter",
    )
    expectations.add_argument(
        "--rejected",
        action="store_true",
        help="verify rejected bad-quantity and bad-arguments calls are reported",
    )
    expectations.add_argument(
        "--collect-only",
        action="store_true",
        help="run a headless baseline scenario and print only keyword names and counts",
    )
    parser.add_argument(
        "--scenario-seconds",
        type=float,
        default=60.0,
        help="duration of the headless baseline scenario (default: 60 seconds)",
    )
    parser.add_argument(
        "--expect-on-demand",
        action="store_true",
        help="require SERV.UNKNOWNREPORT to write before the server shuts down",
    )
    parser.add_argument(
        "--csv",
        action="store_true",
        help="read unknown-keywords.csv instead of the JSON report",
    )
    parser.add_argument(
        "--expect-set",
        action="store_true",
        help="expect one unresolved setter in addition to the four-key probe",
    )
    args = parser.parse_args()

    if args.scenario_seconds < 1 or args.scenario_seconds > 3600:
        parser.error("--scenario-seconds must be between 1 and 3600")

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    report_path = fixture / "logs" / (
        "unknown-keywords.csv" if args.csv else "unknown-keywords.json"
    )
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
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open probe server log: {error}", file=sys.stderr)
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
                    (
                        "Administrator"
                        if args.expect_on_demand or args.collect_only
                        else f"unknown_probe_{os.getpid()}"
                    ),
                    "probe-password",
                    game_port=args.port + 1000,
                )
                if sock is None:
                    raise RuntimeError("probe client did not reach the character list")
                try:
                    sock.sendall(
                        make_char_create(
                            name="UnknownProbe",
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
                        raise RuntimeError("probe character did not enter the game")
                    if args.collect_only:
                        run_headless_scenario(
                            sock,
                            duration=args.scenario_seconds,
                        )
                    if args.expect_on_demand:
                        deadline = time.monotonic() + 5.0
                        while not report_path.is_file() and time.monotonic() < deadline:
                            time.sleep(0.05)
                        if not report_path.is_file():
                            raise RuntimeError(
                                "SERV.UNKNOWNREPORT did not write the report before shutdown"
                            )
                finally:
                    sock.close()
            except (OSError, RuntimeError) as error:
                runner_error = str(error)
            finally:
                try:
                    server_returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    runner_error = runner_error or f"server shutdown failed: {error}"

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        runner_error = runner_error or f"unable to inspect server log: {error}"

    failures = []
    if runner_error:
        failures.append(
            f"unknown-keyword collection failed: {runner_error}"
            if args.collect_only
            else f"unknown-keyword probe failed: {runner_error}"
        )
    shutdown_errors = shutdown_failures(server_returncode, log_contents)
    if shutdown_errors and args.collect_only:
        failures.append("unknown-keyword collection server did not shut down cleanly")
    else:
        failures.extend(shutdown_errors)
    if args.expect_disabled:
        if report_path.exists():
            failures.append("reporting was disabled but a report file was written")
    elif args.overflow:
        failures.extend(overflow_probe_failures(report_path))
    elif not args.collect_only:
        failures.extend(
            probe_failures(
                fixture,
                report_path,
                rejected=args.rejected,
                normalization=args.normalization,
                expect_set=args.expect_set,
                csv_format=args.csv,
            )
        )

    if failures:
        label = "unknown-keyword collection failed" if args.collect_only else "unknown-keyword probe failed"
        print(f"{label}:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        if not args.collect_only:
            print("\n--- probe server log (tail) ---", file=sys.stderr)
            print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1

    if args.expect_disabled:
        print("unknown-keyword default-off probe passed: no report emitted")
    elif args.csv:
        print("unknown-keyword CSV probe passed")
    elif args.collect_only:
        report = json.loads(report_path.read_text(encoding="utf-8"))
        print("kind keyword count")
        for entry in sorted(
            report["entries"],
            key=lambda item: (-item["count"], item["kind"], item["keyword"]),
        )[:50]:
            print(f"{entry['kind']} {entry['keyword']} {entry['count']}")
        print(
            f"distinct {report['distinct']} total {report['total']} "
            f"overflow {report['overflow']}"
        )
    else:
        report = json.loads(report_path.read_text(encoding="utf-8"))
        print(
            "unknown-keyword probe passed: "
            f"{report['distinct']} keys, {report['total']} hits, "
            f"{report['overflow']} overflow"
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
