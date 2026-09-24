#!/usr/bin/env python3
"""Check long unquoted TAG values and legacy named ATTR bits across saves."""

from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from make_fixture import (
    LEGACY_METADATA_ATTR_KEYS,
    LEGACY_METADATA_ITEM_SERIALS,
    LEGACY_METADATA_TAG_KEY,
    LEGACY_METADATA_TAG_VALUE,
    UID_F_ITEM,
)
from test_events_attr_roundtrip import run_generation, sections, sphere_number


def validate(world: str, generation: int) -> list[str]:
    failures: list[str] = []
    items = sections(world, "[WORLDITEM ")
    if len(items) != len(LEGACY_METADATA_ITEM_SERIALS):
        return [
            f"generation {generation}: expected {len(LEGACY_METADATA_ITEM_SERIALS)} items, "
            f"got {len(items)}"
        ]

    by_serial = {}
    for item in items:
        try:
            serial = sphere_number(item["SERIAL"]) & ~UID_F_ITEM
        except (KeyError, ValueError):
            failures.append(f"generation {generation}: item has invalid SERIAL: {item!r}")
            continue
        by_serial[serial] = item

    for serial, attr_key in zip(LEGACY_METADATA_ITEM_SERIALS, LEGACY_METADATA_ATTR_KEYS):
        item = by_serial.get(serial)
        if item is None:
            failures.append(f"generation {generation}: missing item serial {serial}")
            continue
        tag_key = f"TAG.{LEGACY_METADATA_TAG_KEY.upper()}"
        if item.get(tag_key) != LEGACY_METADATA_TAG_VALUE:
            failures.append(
                f"generation {generation}: {tag_key} length/value mismatch: "
                f"got {item.get(tag_key)!r} (length {len(item.get(tag_key, ''))})"
            )
        if "ATTR" not in item:
            failures.append(f"generation {generation}: serial {serial} has no ATTR")
        else:
            try:
                attr_value = sphere_number(item["ATTR"])
            except ValueError:
                attr_value = -1
            if attr_value != 0x0008:
                failures.append(
                    f"generation {generation}: serial {serial} ATTR={item['ATTR']!r}; "
                    "expected MOVE_ALWAYS (0x0008)"
                )
        if attr_key.upper() in item:
            failures.append(
                f"generation {generation}: recognized legacy key {attr_key} was "
                "re-emitted"
            )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2742)
    parser.add_argument("--generations", type=int, default=2)
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
            world, _chars = run_generation(
                fixture, binary, args.port + generation - 1, generation, args.timeout
            )
        except (OSError, RuntimeError, subprocess.SubprocessError) as error:
            failures.append(f"generation {generation}: {error}")
            break
        failures.extend(validate(world, generation))

    if failures:
        for failure in failures:
            print(f"FAIL: {failure}")
        return 1
    print(
        f"legacy metadata round-trip passed: {args.generations} generations, "
        f"{len(LEGACY_METADATA_ITEM_SERIALS)} items retained long TAG and ATTR"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
