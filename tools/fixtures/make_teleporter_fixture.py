#!/usr/bin/env python3
"""Create a synthetic runtime for dynamic and map teleporter movement."""

from __future__ import annotations

import argparse
from pathlib import Path

from make_fixture import write_mul_fixture, write_runtime_files, write_scripts, write_text


ACCOUNT = "FixturePlayer"
PASSWORD = "fixture-pw"
CHAR_SERIAL = 3
TELEPAD_ITEM_ID = 0x0EA4
TELEPAD_SERIAL = 0x40000021
STATIC_SOURCE = (129, 130, 0)
STATIC_DESTINATION = (128, 129, 0)
DYNAMIC_SOURCE = (129, 128, 0)
DYNAMIC_DESTINATION = (128, 127, 0)


def write_scripts_for_teleporters(root: Path) -> None:
    write_scripts(root)
    with (root / "scripts" / "spheretables.scp").open("a", encoding="ascii") as stream:
        stream.write(
            "\n"
            "[TYPEDEF 17]\n"
            "DEFNAME=T_TELEPAD\n"
            f"\n[ITEMDEF 0x{TELEPAD_ITEM_ID:04X}]\n"
            "DEFNAME=SYNTHETIC_TELEPAD\n"
            "NAME=synthetic telepad\n"
            "TYPE=T_TELEPAD\n"
            "\n[TELEPORTERS]\n"
            f"{STATIC_SOURCE[0]},{STATIC_SOURCE[1]},{STATIC_SOURCE[2]}="
            f"{STATIC_DESTINATION[0]},{STATIC_DESTINATION[1]},{STATIC_DESTINATION[2]}="
            "synthetic-static=1\n"
        )


def write_save(root: Path) -> None:
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic teleporter fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDITEM SYNTHETIC_TELEPAD]",
                f"SERIAL={TELEPAD_SERIAL}",
                f"MOREP={DYNAMIC_DESTINATION[0]},{DYNAMIC_DESTINATION[1]},{DYNAMIC_DESTINATION[2]}",
                f"P={DYNAMIC_SOURCE[0]},{DYNAMIC_SOURCE[1]},{DYNAMIC_SOURCE[2]}",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic teleporter fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={CHAR_SERIAL}",
                f"ACCOUNT={ACCOUNT}",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {ACCOUNT}]",
                f"PASSWORD={PASSWORD}",
                "PLEVEL=Admin",
                f"CHARUID={CHAR_SERIAL}",
                f"LASTCHARUID={CHAR_SERIAL}",
                "[EOF]",
            ]
        ),
    )
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")
    write_runtime_files(root)
    write_scripts_for_teleporters(root)
    write_save(root)
    write_mul_fixture(root, extra_item_id=TELEPAD_ITEM_ID)
    print(f"wrote synthetic teleporter fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
