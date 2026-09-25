#!/usr/bin/env python3
"""Run one isolated synthetic fixture case.

The manifest runner calls this for one isolated case.  The case catalogue owns
the generator mode, output directory, port, and checker arguments, so adding a
test does not require editing a shared shell command block.
"""

from __future__ import annotations

import argparse
import os
import shlex
import subprocess
import sys
from pathlib import Path

from fixture_cases import FIXTURE_CASES, FixtureCase


ROOT = Path(__file__).resolve().parent


def _run(command: list[str], *, env: dict[str, str], cwd: Path) -> None:
    print("+ " + " ".join(shlex.quote(part) for part in command), flush=True)
    subprocess.run(command, check=True, cwd=cwd, env=env)


def _case_output(root: Path, case_name: str, case: FixtureCase, variant: str) -> Path | None:
    if case.output is None:
        return None
    prefix = "sphere-asan-" if variant == "asan" else "sphere-"
    return root / f"{prefix}{case.output}"


def _test_command(
    test_script: str,
    fixture: Path | None,
    binary: Path,
    port: int | None,
    test_args: tuple[str, ...],
    repo: Path,
    pass_fixture: bool,
    pass_port: bool,
) -> list[str]:
    command = [sys.executable, str(ROOT / test_script)]
    if pass_fixture:
        if fixture is None:
            raise RuntimeError(f"{test_script} requires a generated fixture")
        command.append(str(fixture))
    command.extend(("--binary", str(binary)))
    if pass_port:
        if port is None:
            raise RuntimeError(f"{test_script} requires a port")
        command.extend(("--port", str(port)))
    command.extend(test_args)
    if test_script == "run_suite.py":
        if "--unknown-keyword-allowlist" in test_args:
            index = command.index("--unknown-keyword-allowlist")
            command.insert(index + 1, str(repo / "tools" / "fixtures" / "unknown_keyword_allowlist.json"))
        command.extend(("--repo", str(repo)))
    return command


def run_case(case_name: str, variant: str, binary: Path, root: Path, repo: Path) -> None:
    case = FIXTURE_CASES[case_name]
    fixture = _case_output(root, case_name, case, variant)
    env = os.environ.copy()
    if variant == "asan":
        env.update(
            {
                "ASAN_OPTIONS": "quarantine_size_mb=64:malloc_context_size=8:detect_leaks=0:abort_on_error=1",
                "UBSAN_OPTIONS": "halt_on_error=1:print_stacktrace=1",
            }
        )

    selected_mode = case.selected_mode(variant)
    if fixture is not None:
        fixture.parent.mkdir(parents=True, exist_ok=True)
    if selected_mode is not None:
        if fixture is None:
            raise RuntimeError(f"case {case_name} selects mode {selected_mode} without output")
        command = [
            sys.executable,
            str(ROOT / "make_fixture.py"),
            str(fixture),
            "--mode",
            selected_mode,
        ]
        _run(command, env=env, cwd=repo)
    elif case.generator != "make_fixture.py":
        if fixture is None:
            raise RuntimeError(f"case {case_name} has a generator but no output")
        _run(
            [
                sys.executable,
                str(ROOT / case.generator),
                str(fixture),
                *case.generator_args,
            ],
            env=env,
            cwd=repo,
        )

    port = case.selected_port(variant)
    test_args = case.selected_test_args(variant)
    for index, test in enumerate(case.tests):
        if index:
            # Two checkers may intentionally consume the same generated save;
            # each still gets its own named CI step/case invocation.
            pass
        command = _test_command(
            test.script,
            fixture,
            binary,
            port,
            (*test.args, *test_args),
            repo,
            test.pass_fixture,
            test.pass_port,
        )
        _run(command, env=env, cwd=repo)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("case", choices=tuple(sorted(FIXTURE_CASES)))
    parser.add_argument("--variant", choices=("native", "asan"), required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()
    repo = ROOT.parents[1]
    try:
        run_case(
            args.case,
            args.variant,
            args.binary.resolve(),
            args.root.resolve(),
            repo,
        )
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        print(f"fixture case {args.case} failed: {error}", file=sys.stderr)
        return getattr(error, "returncode", 1) or 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
