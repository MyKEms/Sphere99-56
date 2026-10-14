"""Synthetic coverage for resource-only MAKEITEM semantics."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ResourceOnlyMakeProbe"
PASSWORD = "res-pw"
EVENT_NAME = "e_ResourceOnlyMakeProbe"
MARKER = "SPHERE_RESOURCE_ONLY_MAKE"
EMPTY_ITEM_ID = 0x0F91
RULE_ITEM_ID = 0x0F92
RESOURCE_ITEM_ID = 0x0F93
RESOURCE_MATERIAL_ID = 0x0F94
MISSING_RESOURCE_ITEM_ID = 0x0F95


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{EMPTY_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_RESOURCE_EMPTY
NAME=synthetic empty make definition
TYPE=T_NORMAL

[ITEMDEF 0x{RULE_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_RESOURCE_RULE
NAME=synthetic skill make definition
TYPE=T_NORMAL
SKILLMAKE=SYNTH_SKILL_0 0.0

[ITEMDEF 0x{RESOURCE_MATERIAL_ID:04X}]
DEFNAME=SYNTHETIC_RESOURCE_MATERIAL
NAME=synthetic material
TYPE=T_NORMAL

[ITEMDEF 0x{MISSING_RESOURCE_ITEM_ID + 1:04X}]
DEFNAME=SYNTHETIC_RESOURCE_MATERIAL_MISSING
NAME=synthetic unavailable material
TYPE=T_NORMAL

[ITEMDEF 0x{RESOURCE_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_RESOURCE_ONLY
NAME=synthetic resource-only make definition
TYPE=T_NORMAL
RESOURCES=1 SYNTHETIC_RESOURCE_MATERIAL

[ITEMDEF 0x{MISSING_RESOURCE_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_RESOURCE_MISSING
NAME=synthetic missing resource definition
TYPE=T_NORMAL
RESOURCES=1 SYNTHETIC_RESOURCE_MATERIAL_MISSING

[FUNCTION f_ResourceOnlyMakeProbe]
SYSMESSAGE {MARKER}|empty=<CANMAKE(SYNTHETIC_RESOURCE_EMPTY)>
SYSMESSAGE {MARKER}|rule=<CANMAKE(SYNTHETIC_RESOURCE_RULE)>
SYSMESSAGE {MARKER}|resource=<CANMAKE(SYNTHETIC_RESOURCE_ONLY)>
SYSMESSAGE {MARKER}|missing=<CANMAKE(SYNTHETIC_RESOURCE_MISSING)>
src.makeitem(SYNTHETIC_RESOURCE_ONLY)
SYSMESSAGE {MARKER}|done
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_ResourceOnlyMakeProbe
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic resource-only MAKEITEM fixture\n"
        "VERSION=0.99z8\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic resource-only MAKEITEM fixture
VERSION=0.99z8
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
P=128,128,0
[WORLDITEM DEFAULTITEM]
SERIAL=4
LAYER=21
CONT=3
[WORLDITEM DEFAULTITEM]
SERIAL=5
ID=0x{RESOURCE_MATERIAL_ID:04X}
AMOUNT=1
CONT=3
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=max(RESOURCE_ITEM_ID, MISSING_RESOURCE_ITEM_ID + 1))
    from .compat_writer import write_stackable_tile
    write_stackable_tile(output / "muls" / "tiledata.mul", RESOURCE_ITEM_ID)
    write_stackable_tile(output / "muls" / "tiledata.mul", RESOURCE_MATERIAL_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="resource-only-makeitem",
        fixture_args=(),
        order=207,
        id_block=152,
        case=FixtureCase(
            name="resource-only-makeitem",
            mode="resource-only-makeitem",
            tests=(TestCase("test_resource_only_makeitem.py", (), True, True),),
            ports={"native": 3212, "asan": 3213},
            output="resource-only-makeitem",
        ),
    )
)
