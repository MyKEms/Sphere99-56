"""Registered fixture for a scripted potion typedef and nested effect trigger."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ScriptPotion"
PASSWORD = "sp-pw"
EVENT_NAME = "e_ScriptedPotionProbe"
ITEM_ID = 0x0EB7
ITEM_NAME = "SYNTHETIC_SCRIPTED_POTION"
TYPE_NAME = "T_SYNTHETIC_SCRIPTED_POTION"
SCRIPT_ITEM_ID = ITEM_ID + 1
SCRIPT_ITEM_NAME = "SYNTHETIC_SCRIPTED_SCRIPT_POTION"
SCRIPT_TYPE_NAME = "T_SYNTHETIC_SCRIPTED_SCRIPT_POTION"
MARKER = "SPHERE_SCRIPTED_POTION"
READY_MARKER = MARKER + " READY"
SCRIPT_READY_MARKER = MARKER + " SCRIPT READY"
HANDLED_MARKER = MARKER + " HANDLED"
EFFECT_MARKER = MARKER + " EFFECT"
BASE_MARKER = MARKER + " BASE"
SCRIPT_HANDLED_MARKER = MARKER + " SCRIPT HANDLED"
SCRIPT_EFFECT_MARKER = MARKER + " SCRIPT EFFECT"
SCRIPT_BASE_MARKER = MARKER + " SCRIPT BASE"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[TYPEDEF 1101]
DEFNAME={TYPE_NAME}
ON=@UserDClick
SRC.SYSMESSAGE {HANDLED_MARKER}
TRIGGER @PotionEffect
RETURN 1

[TYPEDEF 1102]
DEFNAME={SCRIPT_TYPE_NAME}
ON=@UserDClick
SRC.SYSMESSAGE {SCRIPT_HANDLED_MARKER}
TRIGGER @PotionEffect
RETURN 1

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic scripted potion
TYPE={TYPE_NAME}
ON=@UserDClick
SRC.SYSMESSAGE {BASE_MARKER}
RETURN 0
ON=@PotionEffect
SRC.SYSMESSAGE {EFFECT_MARKER}
RETURN 1

[ITEMDEF 0x{SCRIPT_ITEM_ID:04X}]
DEFNAME={SCRIPT_ITEM_NAME}
NAME=synthetic scripted potion from script
TYPE={SCRIPT_TYPE_NAME}
ON=@UserDClick
SRC.SYSMESSAGE {SCRIPT_BASE_MARKER}
RETURN 0
ON=@PotionEffect
SRC.SYSMESSAGE {SCRIPT_EFFECT_MARKER}
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {ITEM_NAME}
LASTNEW.P=128,128,0
SYSMESSAGE {READY_MARKER}|<LASTNEW.SERIAL>
NEWITEM {SCRIPT_ITEM_NAME}
LASTNEW.BOUNCE
SYSMESSAGE {SCRIPT_READY_MARKER}
FINDID({SCRIPT_ITEM_NAME}).DCLICK
RETURN 0
""",
        encoding="ascii",
    )
    accounts = output / "accounts" / "sphereaccu.scp"
    accounts.write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic scripted potion fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic scripted potion fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ScriptedPotionProbeCharacter
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=SCRIPT_ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="scripted-potion",
        fixture_args=(),
        order=213,
        id_block=159,
        case=FixtureCase(
            name="scripted-potion",
            mode="scripted-potion",
            tests=(TestCase("test_scripted_potion.py", (), True, True),),
            ports={"native": 45214, "asan": 45215},
            output="scripted-potion",
        ),
    )
)
