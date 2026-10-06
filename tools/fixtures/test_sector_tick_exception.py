#!/usr/bin/env python3
"""Verify reserved memory objects survive a shadowing script definition."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from test_combat_scheduler import main as run_combat_scheduler


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2960)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--seconds", type=float, default=20.0)
    args = parser.parse_args()

    # Reuse the combat interaction and its packet assertions; this wrapper adds
    # the sector-tick diagnostics that expose the malformed memory subtype.
    result = run_combat_scheduler()
    try:
        log_contents = (args.fixture.resolve() / "server.log").read_text(
            encoding="utf-8", errors="replace"
        )
    except OSError as error:
        print(f"sector tick exception probe failed: unable to read server log: {error}", file=sys.stderr)
        return 1

    unexpected = [
        line
        for line in log_contents.splitlines()
        if "ITEMID_MEMORY is not correct IT_EQ_MEMORY_OBJ type!" in line
        or "Exception in Sector" in line
    ]
    if result or unexpected:
        print("sector tick exception probe failed:", file=sys.stderr)
        if result:
            print(f"- combat scheduler returned {result}", file=sys.stderr)
        for line in unexpected:
            print(f"- unexpected diagnostic: {line}", file=sys.stderr)
        return 1

    print("sector tick exception probe passed: reserved memory subtype remained valid")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
