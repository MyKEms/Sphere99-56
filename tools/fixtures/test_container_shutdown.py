#!/usr/bin/env python3
"""Exercise an event-backed nested-container reparent during shutdown."""

from __future__ import annotations

import argparse
import os
import signal
import socket
import subprocess
import sys
import time
from pathlib import Path

from run_suite import SANITIZER_OUTPUT_RE, wait_for_port


ACCOUNT_NAME = "ContainerShutdownProbe"
LOGIN_VALUE = "container-shutdown-pw"
DESTINATION_UID = 0x40000000 | 210


def collect_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(
        decode_game_response(data), allow_truncated=True
    ):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(
            packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        )
    return messages


def run_probe(fixture: Path, binary: Path, port: int, startup_timeout: float) -> int:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    log_path = fixture / "server.log"
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            env=os.environ.copy(),
        )
        sock: socket.socket | None = None
        data = bytearray()
        failures: list[str] = []
        returncode = -1
        try:
            wait_for_port("127.0.0.1", port, startup_timeout)
            sock, _ = game_connect(
                "127.0.0.1",
                port,
                ACCOUNT_NAME,
                LOGIN_VALUE,
                game_port=port + 1000,
            )
            if sock is None:
                failures.append("probe did not reach the character list")
            else:
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
                initial = recv_until_game_start(sock, timeout=30.0)
                data.extend(initial)
                if not initial or find_start_packet(decode_game_response(initial)) is None:
                    failures.append("probe character did not enter the world")
                else:
                    # Keep the listener alive while CWorld::Close tears down the
                    # sectors; the event callback must be observable before EOF.
                    time.sleep(1.0)
                    process.send_signal(signal.SIGTERM)
                    sock.settimeout(0.2)
                    deadline = time.monotonic() + 15.0
                    while time.monotonic() < deadline:
                        try:
                            chunk = sock.recv(65536)
                        except socket.timeout:
                            if process.poll() is not None:
                                break
                            continue
                        except OSError:
                            break
                        if not chunk:
                            break
                        data.extend(chunk)

            try:
                returncode = process.wait(timeout=15.0)
            except subprocess.TimeoutExpired:
                process.kill()
                returncode = process.wait(timeout=5.0)
                failures.append("server did not exit after bounded shutdown")
        finally:
            if sock is not None:
                sock.close()
            if process.poll() is None:
                process.kill()
                process.wait(timeout=5.0)

    messages = collect_messages(bytes(data))
    event_markers = [message for message in messages if message.startswith("SPHERE_SHUTDOWN_EVENT")]
    if event_markers.count("SPHERE_SHUTDOWN_EVENT") != 1:
        failures.append(
            "nested-container event marker count was "
            f"{event_markers.count('SPHERE_SHUTDOWN_EVENT')}, messages={event_markers!r}"
        )
    moved = [
        message
        for message in event_markers
        if message.startswith("SPHERE_SHUTDOWN_EVENT_MOVED ")
    ]
    moved_uid: int | None = None
    if len(moved) == 1:
        raw_uid = moved[0].split(" ", 1)[1]
        try:
            moved_uid = int(raw_uid, 0)
        except ValueError:
            # Sphere's formatted UID output is a zero-prefixed hexadecimal
            # token (for example, 0400000d2), rather than a Python literal.
            try:
                moved_uid = int(raw_uid, 16)
            except ValueError:
                moved_uid = None
    if moved_uid != DESTINATION_UID:
        failures.append(f"nested-container destination marker was {moved!r}")

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    diagnostics = [
        line for line in log_contents.splitlines() if SANITIZER_OUTPUT_RE.search(line)
    ]
    if diagnostics:
        failures.append(f"server log contains {len(diagnostics)} sanitizer diagnostic line(s)")
        failures.extend(f"  {line}" for line in diagnostics[:20])
    if returncode != 0:
        failures.append(f"server exited with status {returncode} after fixture shutdown")

    if failures:
        print("container-shutdown probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "container-shutdown probe passed: "
        f"event_markers={event_markers!r} returncode={returncode}"
    )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2798)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    return run_probe(
        args.fixture.resolve(),
        args.binary.resolve(),
        args.port,
        args.startup_timeout,
    )


if __name__ == "__main__":
    raise SystemExit(main())
