#!/usr/bin/env python3
"""Check containment and unknown saved-property stability across repeated saves."""

from __future__ import annotations

import argparse
import atexit
import re
import shutil
import sys
import tempfile
from pathlib import Path

from run_suite import shutdown_failures
from test_world_save_roundtrip import run_server


DIAGNOSTICS_RE = re.compile(
    r"world load diagnostics: accepted=\d+ tolerated_legacy=\d+ "
    r"rejected=(\d+) defaulted=\d+ deleted=\d+"
)
LOAD_COUNTS_RE = re.compile(
    r"world load: created_items=(\d+) created_chars=(\d+) "
    r"read_items=(\d+) read_chars=(\d+) allocated_items=(\d+) allocated_chars=(\d+)"
)


def normalized_save(text: str) -> str:
    """Remove fields that intentionally change on every completed save."""

    normalized: list[str] = []
    in_varnames = False
    for line in text.splitlines():
        if line == "[VARNAMES]":
            in_varnames = True
            continue
        if in_varnames and line.startswith("["):
            in_varnames = False
        if in_varnames:
            continue
        if line.startswith(("TIME=", "SAVECOUNT=", "AGE=", "TIMER=")):
            continue
        normalized.append(line)
    return "\n".join(normalized)


def normalized_format_save(text: str) -> str:
    """Canonicalize item section order for the format fixture."""

    return "\n\n".join(sorted(normalized_save(text).split("\n\n")))


def normalized_metadata_char_save(text: str) -> str:
    """Compare persistent character metadata across login save generations.

    Client attach/detach updates EVENTS and the connected FLAGS bit as part of
    each login.  The metadata probe checks those lifecycle paths separately;
    its idempotence comparison must focus on saved character data and TAGs.
    """

    normalized: list[str] = []
    for line in text.splitlines():
        if line.startswith(("TIME=", "SAVECOUNT=", "AGE=", "TIMER=")):
            continue
        if line.startswith(("EVENTS=", "FLAGS=")):
            continue
        normalized.append(line)
    return "\n".join(normalized)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2724)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    parser.add_argument(
        "--format-compat",
        action="store_true",
        help="assert multi REGION.* and map PIN properties survive each save",
    )
    parser.add_argument(
        "--metadata-roundtrip",
        action="store_true",
        help="assert quoted TAG bytes and an explicit DISPID survive the first save",
    )
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_relogin,
        make_char_play,
        recv_until_game_start,
    )

    failures: list[str] = []
    saved_worlds: list[str] = []
    saved_chars: list[str] = []
    startup_logs: list[str] = []
    temporary_copies: list[Path] = []

    def cleanup_temp_copies() -> None:
        for temporary_copy in temporary_copies:
            shutil.rmtree(temporary_copy, ignore_errors=True)
        temporary_copies.clear()

    atexit.register(cleanup_temp_copies)

    def login_existing_character(login_fixture: Path, login_port: int) -> None:
        sock, _, initial = game_relogin(
            args.host,
            login_port,
            "FixturePlayer",
            "fixture-pw",
            game_port=login_port + 1000,
        )
        if sock is None:
            raise RuntimeError("fixture account did not reach the character list")
        try:
            if find_start_packet(initial) is None:
                sock.sendall(make_char_play(0))
                response = recv_until_game_start(sock, timeout=30.0)
                response_valid = bool(
                    response and find_start_packet(decode_game_response(response))
                )
            else:
                response_valid = True
            if not response_valid:
                raise RuntimeError("fixture character did not enter the world")
        finally:
            sock.close()

    def wait_for_new_save(save_fixture: Path, previous_world: str) -> None:
        import time

        world_path = save_fixture / "save" / "sphereworld.scp"
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            try:
                current = world_path.read_text(encoding="ascii", errors="replace")
            except OSError:
                time.sleep(0.1)
                continue
            if args.metadata_roundtrip:
                required_markers = (
                    "[WORLDITEM SYNTHETIC_ROUNDTRIP_ITEM]",
                )
            elif args.format_compat:
                required_markers = (
                    "LEGACY_UNKNOWN=preserve-me",
                    "REGION.FLAGS=0d2",
                    "[WORLDITEM SYNTHETIC_MULTI]",
                    "[WORLDITEM SYNTHETIC_MAP]",
                    "PIN=100,200,5",
                    "PIN=300,400,6",
                )
            else:
                required_markers = (
                    "LEGACY_UNKNOWN=preserve-me",
                    "REGION.FLAGS=0d2",
                    "[WORLDITEM SYNTHETIC_OBJECT]",
                )
            if current != previous_world and "[EOF]" in current and all(
                marker in current for marker in required_markers
            ):
                return
            time.sleep(0.1)
        raise RuntimeError("logout-triggered save did not produce a new world file")

    current_fixture = fixture
    for generation in range(3):
        if generation:
            next_fixture = Path(tempfile.mkdtemp(prefix="sphere-roundtrip-copy."))
            temporary_copies.append(next_fixture)
            shutil.copytree(current_fixture, next_fixture, dirs_exist_ok=True)
            current_fixture = next_fixture
        log_name = "server.log" if generation == 0 else f"server-round-{generation}.log"
        if generation:
            account_path = current_fixture / "accounts" / "sphereaccu.scp"
            try:
                account_text = account_path.read_text(encoding="ascii", errors="replace")
                account_path.write_text(
                    re.sub(r"(?m)^LASTCHARUID=.*$", "LASTCHARUID=0", account_text),
                    encoding="ascii",
                )
            except OSError as error:
                failures.append(f"generation {generation + 1} could not reset login slot: {error}")
        world_path = current_fixture / "save" / "sphereworld.scp"
        try:
            previous_world = world_path.read_text(encoding="ascii", errors="replace")
        except OSError:
            previous_world = ""
        returncode, error, log_contents = run_server(
            fixture=current_fixture,
            binary=binary,
            host=args.host,
            port=args.port + generation,
            startup_timeout=args.startup_timeout,
            log_path=current_fixture / log_name,
            action=(
                lambda port=args.port + generation, run_fixture=current_fixture,
                saved_world=previous_world: (
                    login_existing_character(run_fixture, port),
                    wait_for_new_save(run_fixture, saved_world),
                )
            ),
        )
        if error:
            failures.append(f"generation {generation + 1} failed: {error}")
        failures.extend(shutdown_failures(returncode, log_contents))
        startup_logs.append(log_contents)
        try:
            saved_worlds.append(
                (current_fixture / "save" / "sphereworld.scp").read_text(
                    encoding="ascii", errors="replace"
                )
            )
            saved_chars.append(
                (current_fixture / "save" / "spherechars.scp").read_text(
                    encoding="ascii", errors="replace"
                )
            )
        except OSError as error:
            failures.append(f"generation {generation + 1} save could not be read: {error}")

    load_counts: list[tuple[str, ...]] = []
    rejected_counts: list[int] = []
    for generation, log_contents in enumerate(startup_logs, start=1):
        count_matches = LOAD_COUNTS_RE.findall(log_contents)
        if len(count_matches) != 1:
            failures.append(
                f"generation {generation} did not report exactly one load count line: "
                f"{count_matches!r}"
            )
        else:
            load_counts.append(count_matches[0])
        diagnostics = DIAGNOSTICS_RE.findall(log_contents)
        if len(diagnostics) != 1:
            failures.append(
                f"generation {generation} did not report exactly one diagnostics line: "
                f"{diagnostics!r}"
            )
        else:
            rejected_counts.append(int(diagnostics[0]))
        for marker in ("property rejected", "Invalid container", "Non container", "orphaned objects"):
            if marker in log_contents:
                failures.append(f"generation {generation} logged {marker!r}")

    if load_counts and any(count != load_counts[0] for count in load_counts[1:]):
        failures.append(f"object counts changed across reloads: {load_counts!r}")
    if rejected_counts and any(rejected > rejected_counts[0] for rejected in rejected_counts[1:]):
        failures.append(
            "reload rejected more properties than the first load: "
            f"{rejected_counts!r}"
        )

    for generation, world in enumerate(saved_worlds, start=1):
        if not args.metadata_roundtrip and world.count("LEGACY_UNKNOWN=preserve-me") != 1:
            failures.append(f"generation {generation} dropped the unknown legacy property")
        if args.metadata_roundtrip:
            combined = world + (
                saved_chars[generation - 1] if generation <= len(saved_chars) else ""
            )
            for marker in (
                'Tag.roundtrip="value with trailing space "',
                'Tag.roundtrip="character trailing space "',
                'Tag.empty=""',
                "Tag.numeric=42",
                "Tag.numeric=7",
                "DISPID=0e9b",
            ):
                if combined.count(marker) != 1:
                    failures.append(
                        f"generation {generation} did not preserve exactly one {marker!r}"
                    )
            if "DISPID=?" in combined:
                failures.append(f"generation {generation} rewrote explicit DISPID as '?'")
        elif args.format_compat:
            region_flags_count = world.count("REGION.FLAGS=")
            exact_region_flags_count = world.count("REGION.FLAGS=0d2")
            if region_flags_count != 1 or exact_region_flags_count != 1:
                failures.append(
                    f"generation {generation} did not preserve exactly one REGION.FLAGS=0d2 "
                    f"(keys={region_flags_count}, exact={exact_region_flags_count})"
                )
        elif world.count("REGION.FLAGS=0d2") != 1:
            failures.append(f"generation {generation} dropped REGION.FLAGS")
        if args.metadata_roundtrip:
            pass
        elif args.format_compat:
            if world.count("[WORLDITEM SYNTHETIC_MULTI]") != 1:
                failures.append(f"generation {generation} lost the synthetic multi")
            if world.count("[WORLDITEM SYNTHETIC_MAP]") != 1:
                failures.append(f"generation {generation} lost the synthetic map")
            for pin in ("PIN=100,200,5", "PIN=300,400,6"):
                if world.count(pin) != 1:
                    failures.append(f"generation {generation} did not retain {pin}")
        else:
            if world.count("[WORLDITEM SYNTHETIC_OBJECT]") != 1:
                failures.append(f"generation {generation} lost the contained synthetic item")
            if not re.search(
                r"\[WORLDITEM SYNTHETIC_OBJECT\].*?\nCONT=",
                world,
                flags=re.DOTALL,
            ):
                failures.append(f"generation {generation} did not retain the child CONT relation")

    if len(saved_worlds) == 3:
        normalize = normalized_format_save if args.format_compat else normalized_save
        if normalize(saved_worlds[1]) != normalize(saved_worlds[2]):
            failures.append("normalized second and third saves differ")
        if args.metadata_roundtrip and len(saved_chars) == 3:
            if normalized_metadata_char_save(saved_chars[1]) != normalized_metadata_char_save(
                saved_chars[2]
            ):
                failures.append("normalized second and third character saves differ")

    cleanup_temp_copies()
    atexit.unregister(cleanup_temp_copies)
    if failures:
        print("world round-trip integrity probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1

    print(
        "world round-trip integrity probe passed: "
        + (
            "three bounded saves across two reloads retained TAG bytes and DISPID"
            if args.metadata_roundtrip
            else "three bounded saves across two reloads retained counts, containment, and unknown properties"
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
