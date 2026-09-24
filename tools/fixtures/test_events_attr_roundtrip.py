#!/usr/bin/env python3
"""Check current-format EVENTS, CHANGER, and ATTR across three saves."""

from __future__ import annotations

import argparse
import re
import subprocess
import time
from pathlib import Path

from run_suite import shutdown_failures, stop_server, wait_for_port


EXPECTED_EVENTS = {"e_AllPlayers", "t_fixture_events", "class_fixture"}
SAVECOUNT_RE = re.compile(r"(?m)^SAVECOUNT=(\d+)$")
SANITIZER_RE = re.compile(
    r"AddressSanitizer|UndefinedBehaviorSanitizer|LeakSanitizer|"
    r"ThreadSanitizer|MemorySanitizer|runtime error:",
    re.IGNORECASE,
)


def sections(text: str, prefix: str) -> list[dict[str, str]]:
    """Return the key/value lines from sections whose header starts *prefix*."""

    result: list[dict[str, str]] = []
    current: dict[str, str] | None = None
    for line in text.splitlines():
        if line.startswith("["):
            if current is not None:
                result.append(current)
            current = {} if line.startswith(prefix) else None
            continue
        if current is None or "=" not in line:
            continue
        key, value = line.split("=", 1)
        current[key.upper()] = value
    if current is not None:
        result.append(current)
    return result


def sphere_number(value: str) -> int:
    """Parse decimal and the leading-zero hexadecimal form emitted by Sphere."""

    value = value.strip()
    if value.lower().startswith("0x"):
        return int(value, 16)
    if len(value) > 1 and value.startswith("0") and re.fullmatch(r"0[0-9a-fA-F]+", value):
        return int(value, 16)
    return int(value, 10)


def read_save(path: Path) -> str:
    return path.read_text(encoding="ascii", errors="replace")


def repeated_values(text: str, prefix: str, key: str) -> list[str]:
    """Return repeated property values from matching sections in save order."""

    values: list[str] = []
    in_section = False
    for line in text.splitlines():
        if line.startswith("["):
            in_section = line.startswith(prefix)
            continue
        if in_section and line.startswith(f"{key}="):
            values.append(line.split("=", 1)[1])
    return values


def wait_for_generation(path: Path, previous: str, timeout: float) -> str:
    previous_count_match = SAVECOUNT_RE.search(previous)
    previous_count = int(previous_count_match.group(1)) if previous_count_match else -1
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        try:
            candidate = read_save(path)
        except OSError:
            time.sleep(0.05)
            continue
        match = SAVECOUNT_RE.search(candidate)
        count = int(match.group(1)) if match else -1
        if (
            candidate != previous
            and count > previous_count
            and "[EOF]" in candidate
        ):
            # Let validation name missing metadata on implementations that do
            # not preserve EVENTS or CHANGER instead of timing out here.
            # A save is written in place.  Require one stable read so the
            # assertions never inspect a partially written section.
            time.sleep(0.05)
            try:
                stable = read_save(path)
            except OSError:
                continue
            if stable == candidate:
                return candidate
        time.sleep(0.05)
    raise RuntimeError(f"save did not advance within {timeout:.1f}s: {path}")


def run_generation(
    fixture: Path, binary: Path, port: int, generation: int, timeout: float
) -> tuple[str, str]:
    world_path = fixture / "save" / "sphereworld.scp"
    chars_path = fixture / "save" / "spherechars.scp"
    before_world = read_save(world_path)
    log_path = fixture / f"events-attr-generation-{generation}.log"
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port("127.0.0.1", port, timeout)
            world = wait_for_generation(world_path, before_world, timeout)
            # NPC WORLDCHAR sections are written to sphereworld.scp.  The
            # separate character file can therefore legitimately remain an
            # EOF-only file for this fixture; read it after the world save so
            # the parser also covers a player-file placement if that changes.
            time.sleep(0.05)
            chars = read_save(chars_path)
        finally:
            returncode = stop_server(process)
    log = log_path.read_text(encoding="utf-8", errors="replace")
    failures = shutdown_failures(returncode, log)
    if failures:
        raise RuntimeError("; ".join(failures))
    if SANITIZER_RE.search(log):
        raise RuntimeError(f"generation {generation} log contains sanitizer output")
    return world, chars


def validate_generation(world: str, chars: str, generation: int) -> list[str]:
    failures: list[str] = []
    items = sections(world, "[WORLDITEM ")
    # NPCs are written to the world save; player characters are written to
    # spherechars.scp.  Accept either placement while requiring exactly one.
    characters = sections(world, "[WORLDCHAR ") + sections(chars, "[WORLDCHAR ")
    if len(items) != 1:
        failures.append(f"generation {generation}: expected one item, got {len(items)}")
    if len(characters) != 1:
        failures.append(
            f"generation {generation}: expected one character, got {len(characters)}"
        )
    if len(items) == 1:
        item = items[0]
        item_events = set(repeated_values(world, "[WORLDITEM ", "EVENTS"))
        if item_events != EXPECTED_EVENTS:
            failures.append(f"generation {generation}: item EVENTS={sorted(item_events)!r}")
        if "CHANGER" not in item:
            failures.append(f"generation {generation}: item CHANGER missing")
        elif sphere_number(item["CHANGER"]) != 1234:
            failures.append(f"generation {generation}: item CHANGER={item['CHANGER']!r}")
        if "ATTR" not in item:
            failures.append(f"generation {generation}: item ATTR missing")
        elif sphere_number(item["ATTR"]) != 0x001C:
            failures.append(f"generation {generation}: item ATTR={item['ATTR']!r}")
        if item.get("LEGACY_UNKNOWN") != "keep-item":
            failures.append(
                f"generation {generation}: item legacy key={item.get('LEGACY_UNKNOWN')!r}"
            )
    if len(characters) == 1:
        character = characters[0]
        character_events = set(
            repeated_values(world, "[WORLDCHAR ", "EVENTS")
            + repeated_values(chars, "[WORLDCHAR ", "EVENTS")
        )
        if character_events != EXPECTED_EVENTS:
            failures.append(
                f"generation {generation}: character EVENTS={sorted(character_events)!r}"
            )
        if "CHANGER" not in character:
            failures.append(f"generation {generation}: character CHANGER missing")
        elif sphere_number(character["CHANGER"]) != 1234:
            failures.append(
                f"generation {generation}: character CHANGER={character['CHANGER']!r}"
            )
        if character.get("LEGACY_UNKNOWN") != "keep-char":
            failures.append(
                f"generation {generation}: character legacy key={character.get('LEGACY_UNKNOWN')!r}"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2746)
    parser.add_argument("--generations", type=int, default=3)
    parser.add_argument("--timeout", type=float, default=30.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if args.generations < 2:
        parser.error("at least two generations are required")
    if not fixture.is_dir() or not binary.is_file():
        parser.error("fixture directory or server binary does not exist")

    failures: list[str] = []
    for generation in range(1, args.generations + 1):
        try:
            world, chars = run_generation(
                fixture, binary, args.port + generation - 1, generation, args.timeout
            )
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            failures.append(f"generation {generation}: {error}")
            break
        failures.extend(validate_generation(world, chars, generation))

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(
        f"events/changer/attr round-trip passed: {args.generations} generations, "
        "one item and one character retained metadata"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
