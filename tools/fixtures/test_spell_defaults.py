#!/usr/bin/env python3
"""Verify built-in 0.99z8 spell rows, effect dispatch and partial overrides."""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path

from make_fixture import (
    SPELL_DEFAULT_ACCOUNT,
    SPELL_DEFAULT_MARKER,
    SPELL_DEFAULT_PASSWORD,
)
from run_suite import shutdown_failures, stop_server, wait_for_port


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


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream

    messages = []
    for packet in split_packet_stream(data, allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0]
        messages.append(text.decode("ascii", errors="replace"))
    return messages


def run_generation(fixture: Path, binary: Path, port: int) -> tuple[list[str], str, int | None]:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (
        decode_game_response,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    log_path = fixture / f"spell-defaults-{port}.log"
    response = b""
    process = None
    runner_error = ""
    returncode = None
    with log_path.open("wb") as log_file:
        try:
            process = subprocess.Popen(
                [str(binary), f"-P{port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", port, 120.0)
            sock, _ = game_connect(
                "127.0.0.1",
                port,
                SPELL_DEFAULT_ACCOUNT,
                SPELL_DEFAULT_PASSWORD,
                game_port=port + 1000,
            )
            if sock is None:
                raise RuntimeError("spell-default account did not reach character list")
            try:
                sock.sendall(make_char_play(0))
                response = recv_until_game_start(sock, timeout=30.0)
                response = drain_game_socket(sock, response)
            finally:
                sock.close()
        except (OSError, RuntimeError) as error:
            runner_error = str(error)
        finally:
            if process is not None:
                try:
                    returncode = stop_server(process)
                except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                    runner_error = runner_error or f"server shutdown failed: {error}"
    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures = shutdown_failures(returncode, log_contents)
    if runner_error:
        failures.append(runner_error)
    if failures:
        raise RuntimeError("; ".join(failures) + "\n" + "\n".join(log_contents.splitlines()[-80:]))
    return system_messages(decode_game_response(response)), log_contents, returncode


def marker_rows(messages: list[str]) -> dict[str, str]:
    prefix = SPELL_DEFAULT_MARKER + " C|"
    rows: dict[str, str] = {}
    for message in messages:
        if message.startswith(prefix):
            key, _, value = message[len(prefix):].partition("|")
            rows[key] = value
    return rows


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2799)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    failures: list[str] = []
    try:
        first_messages, first_log, _ = run_generation(fixture, binary, args.port)
        first = marker_rows(first_messages)
        required = {
            "heal": "[Heal|7|In Mani|",
            "numeric": "[Heal|7]",
            "night_sight": "[4|Night Sight]",
            "alias": "[Fire Bolt|Fire Bolt]",
        }
        for key, expected in required.items():
            if not first.get(key, "").startswith(expected):
                failures.append(
                    f"default {key} row was {first.get(key)!r}; expected prefix {expected!r}"
                )
        if SPELL_DEFAULT_MARKER + "_END" not in first_messages:
            failures.append("default spell fixture did not reach its end marker")
        cast = first.get("cast", "")
        match = re.fullmatch(r"\[(\d+)\|(\d+)\]", cast)
        if match is None or int(match.group(1)) <= 40 or int(match.group(2)) != 100:
            failures.append(f"default cast did not apply Heal: {cast!r}")

        # A partial [SPELL 4] section must change only MANAUSE.  This is a
        # second load of the same fixture, so it also exercises idempotence.
        with (fixture / "scripts" / "spheretables.scp").open("a", encoding="ascii") as stream:
            stream.write("\n[SPELL 4]\nMANAUSE=9\n")
        second_messages, second_log, _ = run_generation(fixture, binary, args.port + 1)
        second = marker_rows(second_messages)
        if not second.get("heal", "").startswith("[Heal|9|In Mani|"):
            failures.append(f"partial override did not change only Heal mana: {second.get('heal')!r}")
        second_cast = second.get("cast", "")
        second_match = re.fullmatch(r"\[(\d+)\|(\d+)\]", second_cast)
        if second_match is None or int(second_match.group(1)) <= 40 or int(second_match.group(2)) != 100:
            failures.append(f"partial override lost Heal cast dispatch: {second_cast!r}")
        if SPELL_DEFAULT_MARKER + "_END" not in second_messages:
            failures.append("override spell fixture did not reach its end marker")
    except (OSError, RuntimeError) as error:
        failures.append(str(error))
        first_log = locals().get("first_log", "")
        second_log = locals().get("second_log", "")

    if failures:
        print("spell-defaults probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        for label, log in (("first", first_log), ("second", second_log)):
            if log:
                print(f"\n--- {label} server log (tail) ---", file=sys.stderr)
                print("\n".join(log.splitlines()[-80:]), file=sys.stderr)
        return 1

    print("spell-defaults probe passed: built-in rows, cast dispatch and partial override")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
