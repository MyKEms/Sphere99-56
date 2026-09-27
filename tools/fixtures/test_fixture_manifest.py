#!/usr/bin/env python3
"""Validate the discoverable synthetic fixture manifest."""

from __future__ import annotations

from fixture_cases import FIXTURE_CASES, MODE_GENERATORS, MODES
from modes.fragments.allowlists import unknown_keyword_allowlist
from modes.registry import FixtureMode, validate_modes


def main() -> int:
    errors: list[str] = []
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
