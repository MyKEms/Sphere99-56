"""Registered fixture for handled item typedef triggers."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "TypedefTrigger"
PASSWORD = "typedef-pw"
EVENT_NAME = "e_TypedefTriggerProbe"
# Keep the synthetic visual within the 14-bit tile-data range.  The high
# fixture id used during the first draft is a resource UID, not an item id,
# and is rejected while loading the generated [ITEMDEF].
ITEM_ID = 0x0EAC
ITEM_NAME = "SYNTHETIC_TYPEDEF_TRIGGER_ITEM"
MARKER = "SPHERE_TYPEDEF_TRIGGER"
READY_MARKER = MARKER + " READY"
HANDLED_MARKER = MARKER + " HANDLED"
BASE_MARKER = MARKER + " BASE"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    tables.write_text(
        table_text
        + f"""

[TYPEDEF 1100]
DEFNAME=T_SYNTHETIC_TRIGGER_RETURN
ON=@UserDClick
SRC.SYSMESSAGE {HANDLED_MARKER}
RETURN 1

[ITEMDEF 0x{ITEM_ID:06X}]
DEFNAME={ITEM_NAME}
NAME=synthetic typedef trigger item
TYPE=T_SYNTHETIC_TRIGGER_RETURN
ON=@UserDClick
SRC.SYSMESSAGE {BASE_MARKER}
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {ITEM_NAME}
LASTNEW.P=128,128,0
SYSMESSAGE {READY_MARKER}|<LASTNEW.SERIAL>|<LASTNEW.TYPE>|<LASTNEW.TYPEDEF>
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
        'TITLE="Sphere synthetic typedef trigger fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic typedef trigger fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=TypedefTriggerProbeCharacter
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
    from .compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="typedef-trigger-return",
        fixture_args=(),
        order=210,
        id_block=156,
        case=FixtureCase(
            name="typedef-trigger-return",
            mode="typedef-trigger-return",
            tests=(TestCase("test_typedef_trigger_return.py", (), True, True),),
            ports={"native": 43217, "asan": 43218},
            output="typedef-trigger-return",
        ),
    )
)
