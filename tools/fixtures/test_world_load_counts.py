#!/usr/bin/env python3
"""Check loaded object totals and the admin world-counts method."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, wait_for_port


COUNT_LINE_RE = re.compile(
    r"world load: items=\d+ chars=\d+ read_items=\d+ read_chars=\d+"
)


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages = []
    for packet in split_packet_stream(data):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        message = packet.data[44:].split(b"\0", 1)[0]
        messages.append(message.decode("ascii", errors="replace").strip())
    return messages


def drain_game_socket(sock: socket.socket, initial: bytes) -> bytes:
    data = bytearray(initial)
    deadline = time.monotonic() + 1.5
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
    except (ConnectionResetError, OSError):
        pass
    finally:
        sock.setblocking(True)
    return bytes(data)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2710)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    parser.add_argument(
        "--truncated",
        action="store_true",
        help="expect one incomplete world-item section in the save",
    )
    parser.add_argument(
        "--unresolved-worldchar-type",
        action="store_true",
        help="expect one WORLDCHAR section with no matching CHARDEF",
    )
    parser.add_argument(
        "--noncontainer-reference",
        action="store_true",
        help="expect one nested item to reference a non-container",
    )
    parser.add_argument(
        "--typedef-container-reference",
        action="store_true",
        help="expect a symbolic type from TYPEDEFS to create a container",
    )
    parser.add_argument(
        "--multi-property",
        action="store_true",
        help="expect one synthetic IT_MULTI item to load through its P property",
    )
    args = parser.parse_args()

    if sum(
        (
            args.truncated,
            args.unresolved_worldchar_type,
            args.noncontainer_reference,
            args.typedef_container_reference,
            args.multi_property,
        )
    ) > 1:
        parser.error("choose only one world-load fixture mode")

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    log_path = fixture / "server.log"
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")
    if not (fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {fixture / 'sphere.ini'}")

    if args.noncontainer_reference:
        expected_line = "world load: items=2 chars=1 read_items=3 read_chars=1"
    elif args.unresolved_worldchar_type:
        expected_line = "world load: items=2 chars=0 read_items=2 read_chars=1"
    elif args.truncated:
        expected_line = "world load: items=2 chars=1 read_items=3 read_chars=1"
    else:
        expected_line = "world load: items=2 chars=1 read_items=2 read_chars=1"
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
    startup_log = ""
    startup_lines: list[str] = []
    response_messages: list[str] = []
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        print(f"unable to open world-counts server log: {error}", file=sys.stderr)
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
            runner_error = f"unable to start server: {error}"
        else:
            try:
                wait_for_port(args.host, args.port, args.startup_timeout)
                startup_log = log_path.read_text(encoding="utf-8", errors="replace")
                startup_lines = [
                    match.group(0) for match in COUNT_LINE_RE.finditer(startup_log)
                ]
                startup_errors = [
                    line
                    for line in startup_log.splitlines()
                    if line.startswith("[ERROR]") or line.startswith("[CRITICAL]")
                ]
                if (
                    not args.truncated
                    and not args.unresolved_worldchar_type
                    and not args.noncontainer_reference
                    and startup_errors
                ):
                    raise RuntimeError(
                        f"complete synthetic save logged load errors: {startup_errors!r}"
                    )
                if args.truncated and not any(
                    "world load skipped 1 sections (1 objects)" in line
                    for line in startup_log.splitlines()
                ):
                    raise RuntimeError(
                        "truncated save did not report exactly one skipped object section"
                    )
                if args.unresolved_worldchar_type:
                    if not any(
                        "Invalid WORLDCHAR block index" in line
                        for line in startup_errors
                    ):
                        raise RuntimeError(
                            "unresolved character type did not report an invalid resource index"
                        )
                    if not any(
                        "world load skipped 1 sections (1 objects)" in line
                        for line in startup_log.splitlines()
                    ):
                        raise RuntimeError(
                            "unresolved character type did not report one skipped object section"
                        )
                    diagnostic = (
                        "WORLDCHAR load failed: uid=3 "
                        "type='SYNTHETIC_MISSING_CHARDEF' "
                        "reason=character type does not resolve to a resource index"
                    )
                    if not any(
                        diagnostic in line for line in startup_errors
                    ):
                        raise RuntimeError(
                            "read-but-not-created WORLDCHAR error diagnostic was missing its UID and reason"
                        )
                elif args.noncontainer_reference:
                    diagnostics = [
                        line
                        for line in startup_errors
                        if "Non container uid=" in line
                    ]
                    if len(diagnostics) != 1:
                        raise RuntimeError(
                            "nested non-container reference did not produce exactly one diagnostic"
                    )
                    expected_fragments = (
                        "id=0x0e76",
                        "name=synthetic object",
                        "type=0",
                        "is_container=0",
                        "parent_container_uid=0x00000000",
                        "child_id=0x0e75",
                        "child_name=synthetic container",
                        "child_type=1",
                        "child_is_container=1",
                        "child_container_uid=0x00000000",
                    )
                    missing = [
                        fragment
                        for fragment in expected_fragments
                        if fragment not in diagnostics[0]
                    ]
                    if missing:
                        raise RuntimeError(
                            "nested non-container diagnostic is missing fields: "
                            + ", ".join(missing)
                        )
                    unexpected_errors = [
                        line
                        for line in startup_errors
                        if line not in diagnostics
                    ]
                    if unexpected_errors:
                        raise RuntimeError(
                            "nested non-container fixture logged unexpected errors: "
                            f"{unexpected_errors!r}"
                        )
                elif not args.typedef_container_reference and not args.multi_property:
                    sock, _ = game_connect(
                        args.host,
                        args.port,
                        "Administrator",
                        "world-count-pw",
                        game_port=args.port + 1000,
                    )
                    if sock is None:
                        raise RuntimeError("admin probe did not reach the character list")
                    try:
                        sock.sendall(
                            make_char_create(
                                name="WorldCountProbe",
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
                        response = recv_until_game_start(sock, timeout=10.0)
                        if not response:
                            raise RuntimeError("admin character did not enter the world")
                        decoded = decode_game_response(
                            drain_game_socket(sock, response)
                        )
                        if find_start_packet(decoded) is None:
                            raise RuntimeError("admin character did not enter the world")
                        response_messages = system_messages(decoded)
                    finally:
                        sock.close()
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
        log_contents = ""
        runner_error = runner_error or f"unable to inspect server log: {error}"

    failures = []
    if runner_error:
        failures.append(f"world-load counts probe failed: {runner_error}")
    failures.extend(shutdown_failures(server_returncode, log_contents))

    all_count_lines = [
        match.group(0) for match in COUNT_LINE_RE.finditer(log_contents)
    ]
    if startup_lines != [expected_line]:
        failures.append(
            f"expected one startup line {expected_line!r}, got {startup_lines!r}"
        )
    expected_count_lines = (
        [expected_line]
        if (
            args.unresolved_worldchar_type
            or args.noncontainer_reference
            or args.typedef_container_reference
            or args.multi_property
        )
        else [expected_line, expected_line]
    )
    if all_count_lines != expected_count_lines:
        failures.append(
            "unexpected startup/on-demand log lines: "
            f"expected {expected_count_lines!r}, got {all_count_lines!r}"
        )

    if (
        not args.unresolved_worldchar_type
        and not args.noncontainer_reference
        and not args.typedef_container_reference
        and not args.multi_property
    ):
        admin_lines = [
            message
            for message in response_messages
            if message.startswith("SPHERE_WORLD_COUNTS ")
        ]
        expected_admin_line = f"SPHERE_WORLD_COUNTS {expected_line}"
        if admin_lines != [expected_admin_line]:
            failures.append(
                f"admin SERV.WORLDCOUNTS response was {admin_lines!r}; "
                f"expected {[expected_admin_line]!r}"
            )

    if failures:
        print("world-load counts probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- probe server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        print(f"\n--- admin messages ---\n{response_messages!r}", file=sys.stderr)
        return 1

    print(f"world-load counts probe passed: {expected_line}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
