"""Synthetic coverage for craft-menu eligibility declarations."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture, write_stackable_tile
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "CraftEligProbe"
PASSWORD = "ce-pw"
EVENT_NAME = "e_CraftEligibilityProbe"
MARKER = "SPHERE_CRAFT_ELIGIBILITY"

EMPTY_ID = 0x0F97
RULE_ID = 0x0F98
RESOURCE_ID = 0x0F99
MATERIAL_ID = 0x0F9A
MISSING_ID = 0x0F9B
BACKPACK_ID = 0x0F9C


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{EMPTY_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_NO_RULE
NAME=synthetic craft item without a rule
TYPE=T_NORMAL

[ITEMDEF 0x{RULE_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_RULE
NAME=synthetic craft item with a skill rule
TYPE=T_NORMAL
SKILLMAKE=SYNTH_SKILL_0 0.0

[ITEMDEF 0x{RESOURCE_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_RESOURCE
NAME=synthetic resource-only craft item
TYPE=T_NORMAL
RESOURCES=1 SYNTHETIC_CRAFT_MATERIAL

[ITEMDEF 0x{MATERIAL_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_MATERIAL
NAME=synthetic craft material
TYPE=T_NORMAL

[ITEMDEF 0x{MISSING_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_MISSING
NAME=synthetic craft item with an unavailable material
TYPE=T_NORMAL
RESOURCES=2 SYNTHETIC_CRAFT_MATERIAL

[TYPEDEFS]
T_CONTAINER 1

[ITEMDEF 0x{BACKPACK_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_BACKPACK
NAME=synthetic craft backpack
TYPE=T_CONTAINER
LAYER=21
TDATA2=1

[FUNCTION f_CraftEligibilityProbe]
SYSMESSAGE {MARKER}|empty=<CANMAKE(SYNTHETIC_CRAFT_NO_RULE)>
SYSMESSAGE {MARKER}|rule=<CANMAKE(SYNTHETIC_CRAFT_RULE)>
SYSMESSAGE {MARKER}|resource=<CANMAKE(SYNTHETIC_CRAFT_RESOURCE)>
SYSMESSAGE {MARKER}|missing=<CANMAKE(SYNTHETIC_CRAFT_MISSING)>
SYSMESSAGE {MARKER}|done
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM SYNTHETIC_CRAFT_BACKPACK
EQUIPLAST
NEWITEM SYNTHETIC_CRAFT_MATERIAL
LASTNEW.CONT=<FINDLAYER(21)>
f_CraftEligibilityProbe
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
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
        'TITLE="Sphere synthetic craft eligibility fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic craft eligibility fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME={ACCOUNT}
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
SYNTH_SKILL_0=1000
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=MISSING_ID)
    write_stackable_tile(output / "muls" / "tiledata.mul", MATERIAL_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="craft-eligibility",
        fixture_args=(),
        order=220,
        id_block=166,
        case=FixtureCase(
            name="craft-eligibility",
            mode="craft-eligibility",
            tests=(TestCase("test_craft_eligibility.py", (), True, True),),
            ports={"native": 45226, "asan": 45227},
            output="craft-eligibility",
        ),
    )
)
