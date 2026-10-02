"""Registered fixture for repeated melee swing scheduling."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import (
    write_equipment_tile,
    write_mul_fixture,
    write_runtime_files,
    write_scripts,
)
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "CombatSchedulerProbe"
PASSWORD = "combatpw1"
CHAR_SERIAL = 3
PACK_SERIAL = 4
WEAPON_SERIAL = 5
TARGET_SERIAL = 6
WEAPON_ID = 0x0F51
WEAPON_DEFNAME = "SYNTHETIC_COMBAT_WEAPON"
TARGET_DEFNAME = "SYNTHETIC_COMBAT_TARGET"


def generate(output: Path) -> int:
    """Generate a player with an equipped weapon and a stationary target."""

    write_runtime_files(output)
    write_scripts(output)

    accounts = output / "accounts" / "sphereaccu.scp"
    accounts.write_text(
        f"[{ACCOUNT}]\n"
        f"PASSWORD={PASSWORD}\n"
        f"CHARUID={CHAR_SERIAL}\n"
        f"LASTCHARUID={CHAR_SERIAL}\n"
        "[EOF]\n",
        encoding="ascii",
    )
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[TYPEDEF 74]
DEFNAME=T_EQ_MEMORY_OBJ

[ITEMDEF 0x2007]
DEFNAME=i_memory
TYPE=T_EQ_MEMORY_OBJ
LAYER=30

[ITEMDEF 0x{WEAPON_ID:04X}]
DEFNAME={WEAPON_DEFNAME}
NAME=synthetic combat weapon
TYPE=T_WEAPON_FENCE
DAM=20
SKILL=FENCING
REQSTR=1
TWOHANDS=0
LAYER=1

[CHARDEF {TARGET_DEFNAME}]
DEFNAME={TARGET_DEFNAME}
NAME=synthetic stationary target
ID=0x0190
STR=1
DEX=1
INT=1
HITS=1000
MAXHITS=1000
MANA=0
STAM=1
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text("TITLE=Sphere synthetic combat scheduler fixture\nVERSION=0.99\nSAVECOUNT=0\n[EOF]\n", encoding="ascii")
    chars = output / "save" / "spherechars.scp"
    chars.write_text(
        f"""TITLE=Sphere synthetic combat scheduler fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME=CombatSchedulerPlayer
STR=100
INT=100
DEX=25
HITS=100
MAXHITS=100
MANA=100
STAM=25
P=128,128,0
[WORLDITEM DEFAULTITEM]
SERIAL={PACK_SERIAL}
LAYER=21
CONT={CHAR_SERIAL}
[WORLDITEM SYNTHETIC_COMBAT_WEAPON]
SERIAL={WEAPON_SERIAL}
LAYER=1
CONT={CHAR_SERIAL}
[WORLDCHAR SYNTHETIC_COMBAT_TARGET]
SERIAL={TARGET_SERIAL}
NAME=CombatSchedulerTarget
NPC=2
FLAGS=0x00000004
STR=1
INT=1
DEX=1
HITS=1000
MAXHITS=1000
MANA=0
STAM=1
P=129,128,0
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=WEAPON_ID)
    write_equipment_tile(output / "muls" / "tiledata.mul", WEAPON_ID, 1)
    return 0


MODE = register_mode(
    FixtureMode(
        name="combat-scheduler",
        fixture_args=(),
        order=89,
        id_block=89,
        case=FixtureCase(
            name="combat-scheduler",
            mode="combat-scheduler",
            tests=(TestCase("test_combat_scheduler.py", (), True, True),),
            ports={"native": 2952, "asan": 2953},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="combat-scheduler",
            test_args_by_variant={},
        ),
    )
)
