#!/usr/bin/env python3
"""Verify 0.99 bare account sections, login, save, and reload."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from run_suite import shutdown_failures
from test_world_save_roundtrip import run_server, wait_for_saved_pair

ACCOUNT_NAME = "AccountProbe"
ACCOUNT_PASSWORD = "account_probe_pw"
CHAR_SERIAL = 1
PACK_SERIAL = 2
PACK_UID = 0x40000000 | PACK_SERIAL
ITEM_SERIAL = 3
ITEM_UID = 0x40000000 | ITEM_SERIAL


def _write_fixture(root: Path) -> None:
    subprocess.run(
        [
            sys.executable,
            str(Path(__file__).resolve().parent / "make_fixture.py"),
            str(root),
            "--world-save-probe",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    (root / "accounts" / "sphereaccu.scp").write_text(
        "[AccountProbe]\n"
        f"PASSWORD={ACCOUNT_PASSWORD}\n"
        f"CHARUID={CHAR_SERIAL}\n"
        f"LASTCHARUID={CHAR_SERIAL}\n"
        "[EOF]\n",
        encoding="utf-8",
    )
    (root / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="utf-8")
    (root / "save" / "sphereworld.scp").write_text("[EOF]\n", encoding="utf-8")
    (root / "save" / "spherechars.scp").write_text(
        "TITLE=Sphere 0.99 account fixture\n"
        "VERSION=0.99\n"
        "SAVECOUNT=0\n"
        "[WORLDCHAR c_MAN]\n"
        f"SERIAL={CHAR_SERIAL}\n"
        f"ACCOUNT={ACCOUNT_NAME}\n"
        "NAME=AccountProbeChar\n"
        "STR=100\nINT=100\nDEX=100\nHITS=100\nMAXHITS=100\n"
        "MANA=100\nSTAM=100\nP=130,128,0\n"
        "[WORLDITEM DEFAULTITEM]\n"
        f"SERIAL={PACK_SERIAL}\n"
        "LAYER=21\n"
        f"CONT={CHAR_SERIAL}\n"
        "[WORLDITEM SYNTHETIC_OBJECT]\n"
        f"SERIAL={ITEM_SERIAL}\n"
        f"CONT={PACK_UID}\n"
        "P=12,34,0\n"
        "[EOF]\n",
        encoding="utf-8",
    )


def _login_existing(binary: Path, root: Path, port: int) -> tuple[int | None, str | None, str, str, str]:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    def action() -> None:
        sock, _ = game_connect(
            "127.0.0.1",
            port,
            ACCOUNT_NAME,
            ACCOUNT_PASSWORD,
            game_port=port + 1000,
        )
        if sock is None:
            raise RuntimeError("existing 0.99 account did not reach its character list")
        try:
            sock.sendall(make_char_play(0))
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("existing account character did not enter the world")
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=root,
        binary=binary,
        host="127.0.0.1",
        port=port,
        startup_timeout=180.0,
        log_path=root / f"account-{port}.log",
        action=action,
    )
    chars, world = wait_for_saved_pair(root, 30.0)
    accounts = (root / "accounts" / "sphereaccu.scp").read_text(
        encoding="utf-8", errors="replace"
    )
    return returncode, runner_error, log_contents, accounts, chars + world


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2804)
    args = parser.parse_args()
    binary = args.binary.resolve()

    with tempfile.TemporaryDirectory(prefix="sphere-account-format-") as temporary:
        root = Path(temporary) / "fixture"
        _write_fixture(root)
        failures: list[str] = []
        first = _login_existing(binary, root, args.port)
        failures.extend(
            item
            for item in (
                first[1],
                *shutdown_failures(first[0], first[2]),
            )
            if item
        )
        if not re.search(
            r"accounts load: format=0\.99z8 sections=1 loaded=1 rejected=0",
            first[2],
        ):
            failures.append("first load did not report the 0.99z8 account aggregate")
        if not re.search(rf"(?im)^\[AccountProbe\]\s*$", first[3]):
            failures.append("first save did not retain the bare 0.99 account header")
        if re.search(r"(?im)^\[ACCOUNT\s+AccountProbe\]\s*$", first[3]):
            failures.append("first save rewrote the account as changes-file syntax")
        if len(re.findall(rf"(?im)^CHARUID={CHAR_SERIAL}\s*$", first[3])) != 1:
            failures.append("first save did not retain exactly one existing character UID")
        if not re.search(rf"(?im)^SERIAL=0?4000000{ITEM_SERIAL}\s*$", first[4]):
            failures.append("first save dropped the contained item")
        if not re.search(rf"(?im)^CONT=0?4000000{PACK_SERIAL}\s*$", first[4]):
            failures.append("first save dropped the character/item relation")
        if re.search(r"(?im)^\[AccountProbe\]", (root / "accounts" / "sphereacct.scp").read_text()):
            failures.append("first save left an account section in sphereacct.scp")

        second = _login_existing(binary, root, args.port + 1)
        failures.extend(
            item
            for item in (
                second[1],
                *shutdown_failures(second[0], second[2]),
            )
            if item
        )
        if not re.search(
            r"accounts load: format=0\.99z8 sections=1 loaded=1 rejected=0",
            second[2],
        ):
            failures.append("reload did not report the 0.99z8 account aggregate")
        if not re.search(rf"(?im)^SERIAL=0?4000000{ITEM_SERIAL}\s*$", second[4]):
            failures.append("reload/save round trip dropped the contained item")
        if not re.search(rf"(?im)^CONT=0?4000000{PACK_SERIAL}\s*$", second[4]):
            failures.append("reload/save round trip dropped the character/item relation")

        if failures:
            print("0.99 account load/login/save fixture failed:", file=sys.stderr)
            for failure in failures:
                print(f"- {failure}", file=sys.stderr)
            return 1

    print("0.99 account load/login/save fixture passed: existing character and item survived two saves")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
