#!/usr/bin/env python3
"""Run the registered synthetic fixture manifest for one build variant."""

from __future__ import annotations

import argparse
from pathlib import Path

from fixture_cases import FIXTURE_CASES
from run_fixture_case import run_case


def shard_case_names(shard: int, shards: int) -> tuple[str, ...]:
    """Return one deterministic, one-based shard of the fixture manifest."""

    if shards < 1:
        raise ValueError("shards must be positive")
    if shard < 1 or shard > shards:
        raise ValueError(f"shard must be between 1 and {shards}")
    names = tuple(FIXTURE_CASES)
    return tuple(name for index, name in enumerate(names) if index % shards == shard - 1)


def parse_shard(value: str) -> tuple[int, int]:
    """Parse the CLI's one-based ``I/N`` shard spelling."""

    try:
        shard_text, shards_text = value.split("/", 1)
        shard, shards = int(shard_text), int(shards_text)
    except (ValueError, AttributeError):
        raise ValueError("shard must use the I/N form, for example 1/2") from None
    if shards < 1 or shard < 1 or shard > shards:
        raise ValueError("shard must be between 1 and N")
    return shard, shards


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--variant", choices=("native", "asan"), required=True)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument(
        "--shard",
        default="1/1",
        metavar="I/N",
        help="run one-based shard I of N in manifest order (default: 1/1)",
    )
    args = parser.parse_args()

    repo = Path(__file__).resolve().parents[2]
    binary = args.binary.resolve()
    root = args.root.resolve()
    try:
        shard, shards = parse_shard(args.shard)
        case_names = shard_case_names(shard, shards)
    except ValueError as error:
        parser.error(str(error))
    for case_name in case_names:
        case = FIXTURE_CASES[case_name]
        print(
            f"\n=== fixture case: {case_name} ({args.variant}, shard {shard}/{shards}) ===",
            flush=True,
        )
        run_case(case_name, args.variant, binary, root, repo)
    print(
        f"\nfixture shard passed: {shard}/{shards}, "
        f"{len(case_names)} of {len(FIXTURE_CASES)} cases ({args.variant})"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
