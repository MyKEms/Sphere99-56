#!/usr/bin/env python3
"""Validate the discoverable synthetic fixture manifest."""

from __future__ import annotations

import contextlib
import io
import re
import subprocess
import sys
import tempfile
from pathlib import Path

from fixture_cases import FIXTURE_CASES, MODE_GENERATORS, MODES
from modes.fragments.allowlists import unknown_keyword_allowlist
from modes.registry import FixtureMode, validate_modes

ROOT = Path(__file__).resolve().parent
SAVECOUNT_RE = re.compile(r"^SAVECOUNT=(.*)$", re.MULTILINE)


def _save_count(path: Path) -> str | None:
    if not path.is_file():
        return None
    header = path.read_text(encoding="utf-8", errors="replace").split("[EOF]", 1)[0]
    match = SAVECOUNT_RE.search(header)
    return match.group(1).strip() if match else None


def check_seeded_save_pairs(errors: list[str]) -> None:
    """Every generated fixture must seed a world/chars pair the server loads.

    The server writes the same SAVECOUNT header into both files of a save and
    rejects a pair unless neither file carries one or both carry the same one.
    """

    with tempfile.TemporaryDirectory(prefix="sphere-fixture-pairs-") as temporary:
        root = Path(temporary)
        generated: list[tuple[str, Path]] = []
        for name, generate in sorted(MODE_GENERATORS.items()):
            output = root / f"mode-{name}"
            with contextlib.redirect_stdout(io.StringIO()):
                generate(output)
            generated.append((name, output))
        for name, case in sorted(FIXTURE_CASES.items()):
            if case.generator == "make_fixture.py" or case.output is None:
                continue
            output = root / f"case-{name}"
            subprocess.run(
                [sys.executable, str(ROOT / case.generator), str(output), *case.generator_args],
                check=True,
                stdout=subprocess.DEVNULL,
            )
            generated.append((name, output))
        for name, output in generated:
            world = _save_count(output / "save" / "sphereworld.scp")
            chars = _save_count(output / "save" / "spherechars.scp")
            if world != chars:
                errors.append(
                    f"{name} seeds a save pair the server rejects: "
                    f"world SAVECOUNT={world} chars SAVECOUNT={chars}"
                )


def main() -> int:
    errors: list[str] = []
    check_seeded_save_pairs(errors)
    for mode in MODES:
        if mode.fixture_args is not None and mode.name not in MODE_GENERATORS:
            errors.append(f"{mode.name} has no auto-discovered generator")
        if mode.case is not None:
            if not mode.case.tests:
                errors.append(f"{mode.name} has no test script")
            if any(not test.script for test in mode.case.tests):
                errors.append(f"{mode.name} has an empty test script")
            for test in mode.case.tests:
                if not test.script.endswith(".py"):
                    errors.append(f"{mode.name} test is not a Python script: {test.script}")
    if set(FIXTURE_CASES) != {mode.name for mode in MODES if mode.case is not None}:
        errors.append("fixture case manifest is out of sync with registered modes")
    try:
        validate_modes(
            (
                FixtureMode("overlap-left", id_block=1),
                FixtureMode("overlap-right", id_block=1),
            )
        )
    except ValueError as error:
        if "overlapping synthetic id ranges" not in str(error):
            errors.append(f"overlap guard reported the wrong error: {error}")
    else:
        errors.append("overlap guard accepted overlapping synthetic id ranges")
    allowlist = unknown_keyword_allowlist()
    if [entry.get("keyword") for entry in allowlist["entries"]] != [
        "@ENVIRONCHANGE",
        "@ITEMUNEQUIP",
        "@STEP",
        "@NPCSEENEWPLAYER",
        "@TIMER",
        "@UNEQUIP",
    ]:
        errors.append("unknown-keyword allowlist fragments are not merged deterministically")
    if errors:
        for error in errors:
            print(error)
        return 1
    print(f"fixture manifest valid: {len(MODES)} modes, {len(FIXTURE_CASES)} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
