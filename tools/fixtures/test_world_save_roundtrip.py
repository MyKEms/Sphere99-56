#!/usr/bin/env python3
"""Verify a logged-in player's contained items survive save and reload."""

from __future__ import annotations

import argparse
import ctypes
import os
import re
import select
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path
from typing import Callable, Optional

from run_suite import shutdown_failures, stop_server, wait_for_port


ACCOUNT_NAME = "WorldSaveProbe"
LOGIN_VALUE = "world-save-pw"


def read_saved_pair(fixture: Path) -> tuple[str, str]:
    chars = (fixture / "save" / "spherechars.scp").read_text(
        encoding="ascii", errors="replace"
    )
    world = (fixture / "save" / "sphereworld.scp").read_text(
        encoding="ascii", errors="replace"
    )
    return chars, world


def wait_for_saved_pair(
    fixture: Path, timeout: float, previous: Optional[tuple[str, str]] = None
) -> tuple[str, str]:
    """Return the published (chars, world) pair once a save has completed.

    With ``previous`` (the pair read before the server ran), both files must
    also differ from it, so a seeded pair is never mistaken for a new save.
    """

    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            chars, world = read_saved_pair(fixture)
        except OSError:
            time.sleep(0.1)
            continue
        if (
            len(chars) > len("[EOF]\n")
            and len(world) > len("[EOF]\n")
            and "[EOF]" in chars
            and "[EOF]" in world
            and (
                previous is None
                or (chars != previous[0] and world != previous[1])
            )
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
    watch_path: Optional[Path] = None,
) -> tuple[Optional[int], Optional[str], str]:
    runner_error = None
    server_returncode = None
    log_path.parent.mkdir(parents=True, exist_ok=True)
    try:
        log_file = log_path.open("wb")
    except OSError as error:
        return None, f"unable to open server log: {error}", ""

    watch_stop = threading.Event()
    watch_errors: list[str] = []
    watch_thread: Optional[threading.Thread] = None

    def watch_published_path() -> None:
        if watch_path is None:
            return
        path = watch_path.resolve()
        fd = -1
        try:
            libc = ctypes.CDLL(None, use_errno=True)
            init1 = getattr(libc, "inotify_init1", None)
            add_watch = getattr(libc, "inotify_add_watch", None)
            if init1 is not None and add_watch is not None:
                fd = init1(os.O_NONBLOCK | os.O_CLOEXEC)
                if fd >= 0:
                    mask = 0x00000040 | 0x00000200  # IN_MOVED_FROM | IN_DELETE
                    if add_watch(fd, str(path.parent).encode(), mask) < 0:
                        os.close(fd)
                        fd = -1

            while not watch_stop.is_set():
                if not path.exists():
                    watch_errors.append(f"published file was absent: {path.name}")
                    return
                if fd < 0:
                    watch_stop.wait(0.001)
                    continue
                readable, _, _ = select.select([fd], [], [], 0.05)
                if not readable:
                    continue
                try:
                    data = os.read(fd, 8192)
                except BlockingIOError:
                    continue
                offset = 0
                while offset + 16 <= len(data):
                    _, event_mask, _, name_len = struct.unpack_from(
                        "iIII", data, offset
                    )
                    raw_name = data[offset + 16 : offset + 16 + name_len]
                    name = raw_name.split(b"\0", 1)[0].decode(errors="replace")
                    offset += 16 + name_len
                    if name == path.name and event_mask & (0x00000040 | 0x00000200):
                        watch_errors.append(
                            f"published file was moved/deleted: {path.name}"
                        )
                        return
        finally:
            if fd >= 0:
                os.close(fd)

    if watch_path is not None:
        watch_thread = threading.Thread(target=watch_published_path, daemon=True)
        watch_thread.start()

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

    watch_stop.set()
    if watch_thread is not None:
        watch_thread.join(timeout=2.0)
    if watch_errors:
        runner_error = runner_error or "; ".join(watch_errors)

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
