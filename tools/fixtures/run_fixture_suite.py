#!/usr/bin/env python3
"""Run the registered synthetic fixture manifest for one build variant."""

from __future__ import annotations

import argparse
from pathlib import Path

from fixture_cases import FIXTURE_CASES
from run_fixture_case import run_case


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=("native", "asan"), required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[2]
    binary = args.binary.resolve()
    root = args.root.resolve()
    for case_name, case in FIXTURE_CASES.items():
        print(f"\n=== fixture case: {case_name} ({args.variant}) ===", flush=True)
        run_case(case_name, args.variant, binary, root, repo)
    print(f"\nfixture manifest passed: {len(FIXTURE_CASES)} cases ({args.variant})")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
