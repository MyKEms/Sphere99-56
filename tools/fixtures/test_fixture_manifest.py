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
from modes.registry import FixtureCase, FixtureMode, TestCase, validate_modes, validate_ports
from run_fixture_suite import parse_shard, shard_case_names

ROOT = Path(__file__).resolve().parent
SAVECOUNT_RE = re.compile(r"^\s*SAVECOUNT(?:\s*=\s*|\s+)(.*)$", re.IGNORECASE)


def _save_count(path: Path) -> str | None:
    """Return the header SAVECOUNT the server's pair check reads.

    Like the server, only the header is searched: the keys before the first
    section, or a leading [SPHERE] section.  The key matches in any case.
    """

    if not path.is_file():
        return None
    count = None
    in_header = True
    section_seen = False
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("[EOF]"):
            break
        if line.startswith("["):
            in_header = not section_seen and line[:8].upper() == "[SPHERE]"
            section_seen = True
            continue
        match = SAVECOUNT_RE.match(line) if in_header else None
        if match:
            count = match.group(1).strip()
    return count


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
            if name == "fixture":
                script = output / "scripts" / "spheretables.scp"
                contents = script.read_text(encoding="utf-8", errors="replace")
                for line in (
                    "LASTNEWCHAR.FLAG_STONE=1",
                    "LASTNEWCHAR.FLAG_IMMOBILE=1",
                    "LASTNEWCHAR.TIMER=-1",
                ):
                    if line not in contents:
                        errors.append(
                            "fixture timer owner must be inert in the base world: "
                            f"missing {line}"
                        )


def check_fixture_shards(errors: list[str]) -> None:
    """Every manifest case belongs to exactly one requested shard."""

    manifest = tuple(FIXTURE_CASES)
    for shard_text in ("1/1", "1/2", "2/2", "1/3", "2/3", "3/3"):
        shard, shards = parse_shard(shard_text)
        selected = shard_case_names(shard, shards)
        if len(selected) != len(set(selected)):
            errors.append(f"fixture shard {shard_text} contains a duplicate case")
        if any(name not in manifest for name in selected):
            errors.append(f"fixture shard {shard_text} contains an unknown case")

    first, second = shard_case_names(1, 2), shard_case_names(2, 2)
    if set(first) & set(second):
        errors.append("fixture shards 1/2 and 2/2 overlap")
    if set(first) | set(second) != set(manifest):
        errors.append("fixture shards 1/2 and 2/2 do not cover the manifest")

    for invalid in ("0/2", "3/2", "1/0", "bad", "1"):
        try:
            parse_shard(invalid)
        except ValueError:
            continue
        errors.append(f"invalid fixture shard {invalid!r} was accepted")


def check_fixture_ports(errors: list[str]) -> None:
    """Every case must own a unique listen/game endpoint per build variant."""

    errors.extend(validate_ports(FIXTURE_CASES))
    collision_probe = {
        "left": FixtureCase(
            name="left",
            mode=None,
            tests=(TestCase("left.py"),),
            ports={"native": 6100, "asan": 6101},
        ),
        "right": FixtureCase(
            name="right",
            mode=None,
            tests=(TestCase("right.py"),),
            ports={"native": 7100, "asan": 6101},
        ),
    }
    probe_errors = validate_ports(collision_probe)
    if not any("native port 7100" in error for error in probe_errors):
        errors.append("port guard missed a native listen/game collision")
    if not any("asan port 6101" in error for error in probe_errors):
        errors.append("port guard missed a same-variant listen collision")


def main() -> int:
    errors: list[str] = []
    check_seeded_save_pairs(errors)
    check_fixture_shards(errors)
    check_fixture_ports(errors)
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
        "@SKILLSTART",
        "@SKILLSTROKE",
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
