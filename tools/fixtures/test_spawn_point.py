#!/usr/bin/env python3
"""Verify that a saved named spawn target resolves and ticks once."""

from __future__ import annotations

import argparse
import subprocess
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, tail, wait_for_port


MARKER = "SPHERE_SPAWN_POINT_CREATED"
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2746)
    parser.add_argument("--wait", type=float, default=8.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    log_path = fixture / "server.log"
    failures: list[str] = []
    returncode: int | None = None
    process: subprocess.Popen[bytes] | None = None
    observed = b""
    try:
        with log_path.open("wb") as log_file:
            process = subprocess.Popen(
                [str(binary), f"-P{args.port}"],
                cwd=fixture,
                stdin=subprocess.DEVNULL,
                stdout=log_file,
                stderr=subprocess.STDOUT,
            )
            wait_for_port("127.0.0.1", args.port, 120.0)
            tools_path = Path(__file__).resolve().parents[1]
            sys.path.insert(0, str(tools_path))
            from uo_test_client import (  # pylint: disable=import-outside-toplevel
                decode_game_response,
                find_start_packet,
                game_relogin,
                make_char_play,
                recv_until_game_start,
            )
            sock, _, initial = game_relogin(
                "127.0.0.1",
                args.port,
                "FixturePlayer",
                "fixture-pw",
                game_port=args.port + 1000,
            )
            if sock is None:
                raise RuntimeError("spawn-point probe did not reach the character list")
            try:
                if find_start_packet(initial) is None:
                    sock.sendall(make_char_play(0))
                    entry = recv_until_game_start(sock, timeout=30.0)
                    initial += decode_game_response(entry)
                    if find_start_packet(decode_game_response(entry)) is None:
                        raise RuntimeError("spawn-point probe character did not enter the world")
                observed = bytearray(decode_game_response(initial))
                deadline = time.monotonic() + args.wait
                sock.settimeout(0.2)
                while time.monotonic() < deadline and MARKER.encode() not in decode_game_response(observed):
                    try:
                        chunk = sock.recv(65536)
                    except TimeoutError:
                        continue
                    if not chunk:
                        break
                    observed.extend(decode_game_response(chunk))
            finally:
                sock.close()
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    try:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    except OSError as error:
        log_contents = ""
        failures.append(f"unable to read server log: {error}")

    failures.extend(shutdown_failures(returncode, log_contents))
    from uo_packets import split_packet_stream

    marker_count = 0
    try:
        for packet in split_packet_stream(decode_game_response(bytes(observed)), allow_truncated=True):
            if packet.command != 0x1C or len(packet.data) < 45:
                continue
            message = packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
            if message == MARKER:
                marker_count += 1
    except (ValueError, IndexError) as error:
        failures.append(f"unable to decode client marker stream: {error}")
    if marker_count != 1:
        failures.append(f"spawn target marker count was {marker_count}, expected 1")
    bad_spawn_lines = [line for line in log_contents.splitlines() if "Bad Spawn point" in line]
    if bad_spawn_lines:
        failures.append(f"server logged {len(bad_spawn_lines)} Bad Spawn point diagnostic(s)")
    sanitizer_lines = [
        line for line in log_contents.splitlines() if any(marker in line for marker in SANITIZER_MARKERS)
    ]
    failures.extend(f"server log contains sanitizer output: {line}" for line in sanitizer_lines)

    if failures:
        print("spawn-point probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log tail ---", file=sys.stderr)
        print(tail(log_path), file=sys.stderr)
        return 1

    print("spawn-point probe passed: quoted target resolved and one product spawned")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
