#!/usr/bin/env python3
"""Create a synthetic @Step portal fixture with qualified and unqualified chars."""

from __future__ import annotations

import argparse
from pathlib import Path

from make_fixture import write_mul_fixture, write_runtime_files, write_scripts, write_text


ACCOUNT = "StepPortalProbe"
ACCOUNT_KEY = "step-portal-pw"
UNQUALIFIED_SERIAL = 3
QUALIFIED_SERIAL = 4
PORTAL_ITEM_ID = 0x0EB2
PORTAL_SERIAL = 0x40000021
PORTAL_SOURCE = (129, 128, 0)
PORTAL_DESTINATION = (130, 130, 0)
QUALIFIED_PORTAL_SOURCE = (129, 130, 0)
QUALIFIED_PORTAL_DESTINATION = (130, 132, 0)
QUALIFIED_PORTAL_SERIAL = 0x40000022


def write_scripts_for_step_portal(root: Path) -> None:
    write_scripts(root)
    with (root / "scripts" / "spheretables.scp").open("a", encoding="ascii") as stream:
        stream.write(
            "\n"
            "[TYPEDEF 17]\n"
            "DEFNAME=T_TELEPAD2\n"
            f"[ITEMDEF 0x{PORTAL_ITEM_ID:04X}]\n"
            "DEFNAME=SYNTHETIC_STEP_PORTAL\n"
            "NAME=synthetic step portal\n"
            "TYPE=T_TELEPAD2\n"
            "ON=@Step\n"
            "IF (<SRC.TAG.NATION> != 0) && (<SRC.TAG.RACE> != 0) && (<SRC.TAG.REALM> != 0)\n"
            "SRC.SYSMESSAGE SPHERE_STEP_ALLOWED\n"
            "RETURN 1\n"
            "ENDIF\n"
            "SRC.SYSMESSAGE SPHERE_STEP_BLOCKED\n"
            "RETURN 1\n"
        )


def write_save(root: Path) -> None:
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic @Step portal fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDITEM SYNTHETIC_STEP_PORTAL]",
                f"SERIAL={PORTAL_SERIAL}",
                f"MOREP={PORTAL_DESTINATION[0]},{PORTAL_DESTINATION[1]},{PORTAL_DESTINATION[2]}",
                f"P={PORTAL_SOURCE[0]},{PORTAL_SOURCE[1]},{PORTAL_SOURCE[2]}",
                "[WORLDITEM SYNTHETIC_STEP_PORTAL]",
                f"SERIAL={QUALIFIED_PORTAL_SERIAL}",
                f"MOREP={QUALIFIED_PORTAL_DESTINATION[0]},{QUALIFIED_PORTAL_DESTINATION[1]},{QUALIFIED_PORTAL_DESTINATION[2]}",
                f"P={QUALIFIED_PORTAL_SOURCE[0]},{QUALIFIED_PORTAL_SOURCE[1]},{QUALIFIED_PORTAL_SOURCE[2]}",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic @Step portal fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={UNQUALIFIED_SERIAL}",
                f"ACCOUNT={ACCOUNT}",
                "NAME=StepPortalUnqualified",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={QUALIFIED_SERIAL}",
                f"ACCOUNT={ACCOUNT}",
                "NAME=StepPortalQualified",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                f"P={QUALIFIED_PORTAL_SOURCE[0]-1},{QUALIFIED_PORTAL_SOURCE[1]},0",
                "Tag.nation=1",
                "Tag.race=1",
                "Tag.realm=1",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {ACCOUNT}]",
                f"PASSWORD={ACCOUNT_KEY}",
                "PLEVEL=Admin",
                f"CHARUID={UNQUALIFIED_SERIAL}",
                f"CHARUID={QUALIFIED_SERIAL}",
                f"LASTCHARUID={UNQUALIFIED_SERIAL}",
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
    write_scripts_for_step_portal(root)
    write_save(root)
    write_mul_fixture(root, extra_item_id=PORTAL_ITEM_ID)
    print(f"wrote synthetic step portal fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
