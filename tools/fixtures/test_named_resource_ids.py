#!/usr/bin/env python3
"""Require stable named ITEMDEF and CHARDEF IDs across release loads."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
import time
from pathlib import Path


RESOURCE_ID_RE = re.compile(
    r"^(SYNTHETIC_ALLOC_(?:ITEM_\d{2}|CHAR_\d{2}|CONTAINER|CONTENT|"
    r"RESERVED_ITEM|RESERVED_CHAR))=(-?\d+)$"
)


def expected_names() -> set[str]:
    names = {
        f"SYNTHETIC_ALLOC_ITEM_{index:02d}" for index in range(12)
    }
    names.update(
        f"SYNTHETIC_ALLOC_CHAR_{index:02d}" for index in range(12)
    )
    names.update(
        (
            "SYNTHETIC_ALLOC_CONTAINER",
            "SYNTHETIC_ALLOC_CONTENT",
            "SYNTHETIC_ALLOC_RESERVED_ITEM",
            "SYNTHETIC_ALLOC_RESERVED_CHAR",
        )
    )
    return names


def read_resource_ids(path: Path) -> dict[str, int]:
    resources = {}
    for line in path.read_text(encoding="ascii", errors="replace").splitlines():
        match = RESOURCE_ID_RE.fullmatch(line)
        if match:
            name, raw_id = match.groups()
            resources[name] = int(raw_id)
    return resources


def load_resource_id_table(binary: Path, fixture: Path) -> dict[str, int]:
    result = subprocess.run(
        [str(binary), "-D1", "-Q"],
        cwd=fixture,
        check=False,
        capture_output=True,
        text=True,
        timeout=120,
    )
    if result.returncode != 255:
        raise RuntimeError(
            "resource-table load did not exit through -Q (status "
            f"{result.returncode}):\n{result.stdout}{result.stderr}"
        )

    dump_path = fixture / "dumpdefs.txt"
    if not dump_path.is_file():
        raise RuntimeError("-D1 did not write dumpdefs.txt")
    table = read_resource_ids(dump_path)
    missing = sorted(expected_names() - table.keys())
    if missing:
        raise RuntimeError(
            "resource dump is missing named fixture entries: " + ", ".join(missing)
        )
    return table


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")
    if not (fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {fixture / 'sphere.ini'}")

    try:
        first = load_resource_id_table(binary, fixture)
        time.sleep(0.05)
        second = load_resource_id_table(binary, fixture)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        print(f"named resource ID probe failed: {error}", file=sys.stderr)
        return 1

    expected = expected_names()
    first = {name: first[name] for name in expected}
    second = {name: second[name] for name in expected}
    if first != second:
        changed = [
            (name, first[name], second[name])
            for name in sorted(expected)
            if first[name] != second[name]
        ]
        print("named resource ID table changed between release loads:", file=sys.stderr)
        for name, first_id, second_id in changed:
            print(f"  {name}: {first_id} -> {second_id}", file=sys.stderr)
        return 1

    item_ids = [
        first[name]
        for name in expected
        if name.startswith("SYNTHETIC_ALLOC_ITEM_")
        or name
        in (
            "SYNTHETIC_ALLOC_CONTAINER",
            "SYNTHETIC_ALLOC_CONTENT",
            "SYNTHETIC_ALLOC_RESERVED_ITEM",
        )
    ]
    if len(item_ids) != len(set(item_ids)):
        print("named ITEMDEF entries do not have unique IDs", file=sys.stderr)
        return 1
    char_ids = [
        first[name]
        for name in expected
        if name.startswith("SYNTHETIC_ALLOC_CHAR_")
        or name == "SYNTHETIC_ALLOC_RESERVED_CHAR"
    ]
    if len(char_ids) != len(set(char_ids)):
        print("named CHARDEF entries do not have unique IDs", file=sys.stderr)
        return 1

    print(f"named resource IDs stable across two loads ({len(expected)} entries)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
