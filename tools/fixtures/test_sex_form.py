#!/usr/bin/env python3
"""Check comma and space-separated SEX forms for both character sexes."""

from __future__ import annotations

import argparse
import shutil
import socket
import sys
import tempfile
import time
from pathlib import Path

from modes.sex_form import ACCOUNT, FEMALE_SERIAL, MALE_SERIAL, MARKER, PASSWORD
from run_suite import shutdown_failures


EXPECTED = {
    "male": [
        f"{MARKER} space=[Male]",
        f"{MARKER} comma=[Male]",
        "Ziskal jsi 20 zkusenosti.",
    ],
    "female": [
        f"{MARKER} space=[Female]",
        f"{MARKER} comma=[Female]",
        "Ziskala jsi 20 zkusenosti.",
    ],
}


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages: list[str] = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command == 0x1C and len(packet.data) >= 45:
            messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def _login_and_collect(host: str, port: int, account: str, password: str, sex: str) -> list[str]:
    from uo_test_client import game_connect, make_char_play, recv_until_game_start

    sock, _ = game_connect(host, port, account, password, game_port=port + 1000)
    if sock is None:
        raise RuntimeError(f"{account} did not reach its character list")
    try:
        sock.sendall(make_char_play(0))
        data = bytearray(recv_until_game_start(sock, timeout=30.0))
        if not data:
            raise RuntimeError(f"{account} did not enter the world")
        deadline = time.monotonic() + 8.0
        sock.settimeout(0.2)
        while time.monotonic() < deadline:
            messages = system_messages(bytes(data))
            if all(expected in messages for expected in EXPECTED[sex]):
                return messages
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                break
            if not chunk:
                break
            data.extend(chunk)
        return system_messages(bytes(data))
    finally:
        sock.close()


def _keep_character(fixture: Path, serial: int) -> None:
    chars_path = fixture / "save" / "spherechars.scp"
    source = chars_path.read_text(encoding="ascii")
    sections = source.split("[WORLDCHAR ")
    chosen = [section for section in sections[1:] if f"SERIAL={serial}\n" in section]
    if len(chosen) != 1:
        raise RuntimeError(f"fixture did not contain exactly one character serial {serial}")
    chars_path.write_text(
        sections[0] + "[WORLDCHAR " + chosen[0].split("[EOF]", 1)[0] + "[EOF]\n",
        encoding="ascii",
    )
    (fixture / "accounts" / "sphereaccu.scp").write_text(
        f"[ACCOUNT {ACCOUNT}]\nPASSWORD={PASSWORD}\nCHARUID={serial}\nLASTCHARUID={serial}\n[EOF]\n",
        encoding="ascii",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2972)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server

    results: dict[str, list[str]] = {}
    failures: list[str] = []

    # A server keeps a recently disconnected account in its client-linger
    # table.  Run each sex in a fresh bounded server instance and a copied
    # fixture so the rows test character evaluation rather than reconnect or
    # save state.
    for index, (sex, serial) in enumerate((
        ("male", MALE_SERIAL),
        ("female", FEMALE_SERIAL),
    )):
        isolated_root = Path(tempfile.mkdtemp(prefix=f"sex-form-{sex}-", dir=fixture.parent))
        try:
            isolated_fixture = isolated_root / fixture.name
            shutil.copytree(fixture, isolated_fixture)
            _keep_character(isolated_fixture, serial)
            run_port = args.port + index * 2

            def exercise(sex=sex, run_port=run_port) -> None:
                results[sex] = _login_and_collect(args.host, run_port, ACCOUNT, PASSWORD, sex)

            returncode, runner_error, log_contents = run_server(
                fixture=isolated_fixture,
                binary=binary,
                host=args.host,
                port=run_port,
                startup_timeout=args.startup_timeout,
                log_path=isolated_fixture / "server.log",
                action=exercise,
            )
            if runner_error:
                failures.append(f"{sex}: {runner_error}")
            failures.extend(f"{sex}: {failure}" for failure in shutdown_failures(returncode, log_contents))
        finally:
            shutil.rmtree(isolated_root, ignore_errors=True)

    for sex, expected in EXPECTED.items():
        messages = results.get(sex, [])
        missing = [value for value in expected if value not in messages]
        if missing:
            failures.append(f"{sex} missing {missing!r}; messages={messages!r}")

    if failures:
        print("SEX form probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("SEX form probe passed: space/comma and racemessage forms for male/female")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
