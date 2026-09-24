#!/usr/bin/env python3
"""Verify a logged-in player's contained items survive save and reload."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from run_suite import shutdown_failures, stop_server, wait_for_port


ACCOUNT_NAME = "WorldSaveProbe"
LOGIN_VALUE = "world-save-pw"


def wait_for_saved_pair(fixture: Path, timeout: float) -> tuple[str, str]:
    chars_path = fixture / "save" / "spherechars.scp"
    world_path = fixture / "save" / "sphereworld.scp"
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chars = chars_path.read_text(encoding="ascii", errors="replace")
            world = world_path.read_text(encoding="ascii", errors="replace")
        except OSError:
            time.sleep(0.1)
            continue
        if (
            len(chars) > len("[EOF]\n")
            and len(world) > len("[EOF]\n")
            and "[EOF]" in chars
            and "[EOF]" in world
        ):
            return chars, world
        time.sleep(0.1)
    raise RuntimeError("logout-triggered world save did not finish")


def run_server(
    *,
    fixture: Path,
    binary: Path,
    host: str,
    port: int,
    startup_timeout: float,
    log_path: Path,
    action: Callable[[], None],
) -> tuple[Optional[int], Optional[str], str]:
    runner_error = None
    server_returncode = None
    log_path.parent.mkdir(parents=True, exist_ok=True)
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
            runner_error = f"unable to start server: {error}"
        else:
            try:
                wait_for_port(host, port, startup_timeout)
                action()
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
    return server_returncode, runner_error, log_contents


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2720)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
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
        make_char_play,
        recv_until_game_start,
    )

    failures: list[str] = []
    saved_chars = ""

    def create_probe_character() -> None:
        nonlocal saved_chars
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT_NAME,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("save probe did not reach the character list")
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
                raise RuntimeError("probe character did not enter the world")
        finally:
            sock.close()
        saved_chars, _ = wait_for_saved_pair(fixture, 30.0)

    save_returncode, save_error, save_log = run_server(
        fixture=fixture,
        binary=binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=fixture / "server.log",
        action=create_probe_character,
    )
    if save_error:
        failures.append(f"world-save round-trip probe failed: {save_error}")
    failures.extend(shutdown_failures(save_returncode, save_log))
    if "in Write Object" in save_log:
        failures.append("server logged a caught exception while saving an object")

    char_types = re.findall(r"(?m)^\[WORLDCHAR (.*)\]$", saved_chars)
    item_types = re.findall(r"(?m)^\[WORLDITEM (.*)\]$", saved_chars)
    if "c_MAN" not in char_types:
        failures.append(f"saved character type name was not preserved: {char_types!r}")
    if not item_types:
        failures.append("saved character has no contained item sections")
    elif any(not item_type.strip() for item_type in item_types):
        failures.append("one or more contained item type names were not preserved")
    for event_name in ("e_AllPlayers", "spk_AllPlayers"):
        event_lines = re.findall(rf"(?m)^EVENTS={re.escape(event_name)}$", saved_chars)
        if len(event_lines) > 1:
            failures.append("saved character duplicated its standard event: " + event_name)

    reload_succeeded = False
    reload_log = ""
    if not failures:

        def log_in_saved_character() -> None:
            nonlocal reload_succeeded
            sock, _ = game_connect(
                args.host,
                args.port,
                ACCOUNT_NAME,
                LOGIN_VALUE,
                game_port=args.port + 1000,
            )
            if sock is None:
                raise RuntimeError("saved account did not reach the character list")
            try:
                sock.sendall(make_char_play(0))
                response = recv_until_game_start(sock, timeout=30.0)
                reload_succeeded = bool(
                    response and find_start_packet(decode_game_response(response))
                )
                if not reload_succeeded:
                    raise RuntimeError("saved character did not load into the world")
            finally:
                sock.close()

        reload_returncode, reload_error, reload_log = run_server(
            fixture=fixture,
            binary=binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=fixture / "server-reload.log",
            action=log_in_saved_character,
        )
        if reload_error:
            failures.append(f"world reload/login probe failed: {reload_error}")
        failures.extend(shutdown_failures(reload_returncode, reload_log))
        if "in Write Object" in reload_log:
            failures.append("server logged a caught exception while resaving the reloaded character")

    if failures:
        print("world-save round-trip probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        for name, log_contents in (("save", save_log), ("reload", locals().get("reload_log", ""))):
            if log_contents:
                print(f"\n--- {name} server log (tail) ---", file=sys.stderr)
                print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1

    print(
        "world-save round-trip probe passed: "
        f"reloaded the saved player with {len(item_types)} contained item section(s)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
