#!/usr/bin/env python3
"""Exercise spawn-gem save uniqueness and duplicate-serial load handling."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path
from typing import Callable, Optional

from run_suite import shutdown_failures, stop_server, wait_for_port
from test_world_save_roundtrip import wait_for_saved_pair


ACCOUNT_NAME = "WorldSaveProbe"
LOGIN_VALUE = "world-save-pw"
SPAWN_SECTION = "SYNTHETIC_SPAWN_GEM"
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")


def run_server(
    *,
    fixture: Path,
    binary: Path,
    port: int,
    log_name: str,
    action: Callable[[], None],
) -> tuple[Optional[int], Optional[str], str]:
    log_path = fixture / log_name
    runner_error: Optional[str] = None
    returncode: Optional[int] = None
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        return None, f"unable to open server log: {error}", ""
    with log_file:
        try:
            process = subprocess.Popen(
                [str(binary), f"-P{port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
        except OSError as error:
            return None, f"unable to start server: {error}", ""
        try:
            wait_for_port("127.0.0.1", port, 120.0)
            action()
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            runner_error = str(error)
        finally:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                runner_error = runner_error or f"server shutdown failed: {error}"
    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        runner_error = runner_error or f"unable to inspect server log: {error}"
    return returncode, runner_error, log_contents


def create_probe_character(fixture: Path, port: int) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        "127.0.0.1",
        port,
        ACCOUNT_NAME,
        LOGIN_VALUE,
        game_port=port + 1000,
    )
    if sock is None:
        raise RuntimeError("spawn-gem save probe did not reach the character list")
    try:
        sock.sendall(
            make_char_create(
                name=ACCOUNT_NAME,
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
        response = recv_until_game_start(sock, timeout=30.0)
        if not response or find_start_packet(decode_game_response(response)) is None:
            raise RuntimeError("spawn-gem save probe character did not enter the world")
    finally:
        sock.close()

    wait_for_saved_pair(fixture, 30.0)


def login_probe_character(port: int) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        "127.0.0.1",
        port,
        ACCOUNT_NAME,
        LOGIN_VALUE,
        game_port=port + 1000,
    )
    if sock is None:
        raise RuntimeError("spawn-gem reload did not reach the character list")
    try:
        sock.sendall(make_char_play(0))
        response = recv_until_game_start(sock, timeout=30.0)
        if not response or find_start_packet(decode_game_response(response)) is None:
            raise RuntimeError("spawn-gem saved character did not enter the world")
    finally:
        sock.close()


def spawn_sections(world_text: str) -> list[tuple[str, str]]:
    sections = re.split(r"(?m)^(?=\[WORLDITEM )", world_text)
    result: list[tuple[str, str]] = []
    for section in sections:
        if f"[WORLDITEM {SPAWN_SECTION}]" not in section:
            continue
        match = re.search(r"(?m)^SERIAL=([^\r\n]+)$", section)
        if match:
            result.append((match.group(1).strip().lower(), section))
    return result


def sanitizer_failures(log: str) -> list[str]:
    return [
        line for line in log.splitlines() if any(marker in line for marker in SANITIZER_MARKERS)
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2740)
    parser.add_argument("--expected-spawn-gems", type=int, default=10)
    parser.add_argument("--duplicate-load", action="store_true")
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    failures: list[str] = []
    first_log = ""
    returncode, error, first_log = run_server(
        fixture=fixture,
        binary=binary,
        port=args.port,
        log_name="server.log",
        action=(
            (lambda: None)
            if args.duplicate_load
            else lambda: create_probe_character(fixture, args.port)
        ),
    )
    if error:
        failures.append(error)
    failures.extend(shutdown_failures(returncode, first_log))
    failures.extend(
        f"server log contains sanitizer output: {line}"
        for line in sanitizer_failures(first_log)
    )

    world_path = fixture / "save" / "sphereworld.scp"
    try:
        world_text = world_path.read_text(encoding="ascii", errors="replace")
    except OSError as error:
        world_text = ""
        failures.append(f"unable to read saved world: {error}")

    sections = spawn_sections(world_text)
    if args.duplicate_load:
        expected_duplicates = args.expected_spawn_gems
        match = re.search(r"world load duplicate serials: count=(\d+)", first_log)
        if not match:
            failures.append("duplicate-serial load did not report its bounded count")
        elif int(match.group(1)) != expected_duplicates:
            failures.append(
                f"duplicate-serial count was {match.group(1)}, expected {expected_duplicates}"
            )
        if len(sections) != expected_duplicates * 2:
            failures.append(
                f"duplicate fixture was unexpectedly rewritten: found {len(sections)} sections"
            )
        if "Non container" in first_log:
            failures.append("duplicate-serial load emitted a Non container rejection")
        if failures:
            print("spawn-gem duplicate-serial probe failed:", file=sys.stderr)
            for failure in failures:
                print(f"- {failure}", file=sys.stderr)
            print("\n--- server log ---", file=sys.stderr)
            print(first_log, file=sys.stderr)
            return 1
        print(
            "spawn-gem duplicate-serial probe passed: "
            f"kept {expected_duplicates} first objects and reported all duplicates"
        )
        return 0

    if len(sections) != args.expected_spawn_gems:
        failures.append(
            f"save wrote {len(sections)} spawn-gem sections, expected {args.expected_spawn_gems}"
        )
    serials = [serial for serial, _ in sections]
    if len(serials) != len(set(serials)):
        failures.append("saved spawn-gem serials are duplicated")
    if "Non container" in first_log:
        failures.append("spawn-gem save logged a Non container rejection")

    reload_log = ""
    if not failures:
        reload_returncode, reload_error, reload_log = run_server(
            fixture=fixture,
            binary=binary,
            port=args.port,
            log_name="server-reload.log",
            action=lambda: login_probe_character(args.port),
        )
        if reload_error:
            failures.append(reload_error)
        failures.extend(shutdown_failures(reload_returncode, reload_log))
        failures.extend(
            f"reload log contains sanitizer output: {line}"
            for line in sanitizer_failures(reload_log)
        )
        if "Non container" in reload_log:
            failures.append("spawn-gem reload emitted a Non container rejection")

    if failures:
        print("spawn-gem serialization probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        for name, log in (("save", first_log), ("reload", reload_log)):
            if log:
                print(f"\n--- {name} server log (tail) ---", file=sys.stderr)
                print("\n".join(log.splitlines()[-80:]), file=sys.stderr)
        return 1

    print(
        "spawn-gem serialization probe passed: "
        f"{len(sections)} unique sections survived save and reload"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
