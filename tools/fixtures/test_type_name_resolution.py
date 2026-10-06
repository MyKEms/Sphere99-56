#!/usr/bin/env python3
"""Check legacy type and resource aliases during script/world loading."""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

from run_suite import shutdown_failures


ERROR_PATTERNS = (
    re.compile(r"Ignoring invalid item type -1", re.IGNORECASE),
    re.compile(r"Unknown item TYPE -1", re.IGNORECASE),
    re.compile(r"Bad resource list id '(?:T_EERIE_STUFF|T_MAGIC)'", re.IGNORECASE),
    re.compile(r"Bad resource list id 'RANDOM_REAGENT_NECRO'", re.IGNORECASE),
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=4596)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not (fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {fixture / 'sphere.ini'}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server

    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=fixture / "server.log",
        action=lambda: None,
    )
    failures: list[str] = []
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))
    for pattern in ERROR_PATTERNS:
        matches = pattern.findall(log_contents)
        if matches:
            failures.append(f"legacy alias diagnostic {pattern.pattern!r}: {len(matches)}")

    if failures:
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("type/resource alias resolution probe passed: 4/4 diagnostics absent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
