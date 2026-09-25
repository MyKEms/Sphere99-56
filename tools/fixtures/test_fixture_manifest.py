#!/usr/bin/env python3
"""Validate the discoverable synthetic fixture manifest."""

from __future__ import annotations

from fixture_cases import FIXTURE_CASES, MODES


def main() -> int:
    errors: list[str] = []
    ranges: dict[int, str] = {}
    for mode in MODES:
        if mode.id_block in ranges:
            errors.append(
                f"{mode.name} shares synthetic id block with {ranges[mode.id_block]}"
            )
        ranges[mode.id_block] = mode.name
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
    if errors:
        for error in errors:
            print(error)
        return 1
    print(f"fixture manifest valid: {len(MODES)} modes, {len(FIXTURE_CASES)} cases")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
