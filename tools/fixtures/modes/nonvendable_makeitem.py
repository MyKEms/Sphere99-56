"""Synthetic coverage for non-vendable skill-made item delivery."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NonVendableMakeProbe"
LOGIN_VALUE = "nonvendable-pw"
EVENT_NAME = "e_NonVendableMakeProbe"
MARKER = "SPHERE_NONVENDABLE_MAKE"
ITEM_ID = 0x0F96
ITEM_NAME = "SYNTHETIC_NONVENDABLE_CRAFT"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic non-vendable craft result
TYPE=T_NORMAL
VALUE=0
SKILLMAKE=0.0 SYNTH_SKILL_37

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|started
SYSMESSAGE {MARKER}|skill=<SYNTH_SKILL_37>|can=<CANMAKE({ITEM_NAME})>
SYSMESSAGE {MARKER}|call=[<src.makeitem({ITEM_NAME})>]
SYSMESSAGE {MARKER}|after|action=<ACTION>
RETURN 0

ON=@SkillMakeItem
SYSMESSAGE {MARKER}|success|uid=<ARGO.SERIAL>|type=<ARGO.TYPE>
SYSMESSAGE {MARKER}|done
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic non-vendable craft fixture\n"
        "VERSION=0.99z8\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic non-vendable craft fixture
VERSION=0.99z8
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=NonVendableMakeProbe
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
SYNTH_SKILL_37=1000
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="nonvendable-makeitem",
        fixture_args=(),
        order=208,
        id_block=154,
        case=FixtureCase(
            name="nonvendable-makeitem",
            mode="nonvendable-makeitem",
            tests=(TestCase("test_nonvendable_makeitem.py", (), True, True),),
            ports={"native": 43213, "asan": 43214},
            output="nonvendable-makeitem",
        ),
    )
)
