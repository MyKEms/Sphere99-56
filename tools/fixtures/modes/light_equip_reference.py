"""Registered fixture for equipping a live ``LASTNEW`` reference."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "LightEquipReferenceProbe"
PASSWORD = "lepw"
EVENT_NAME = "e_LightEquipReferenceProbe"
MARKER = "SPHERE_LIGHT_EQUIPPED"
READY_MARKER = MARKER + "_READY"
ITEM_ID = 0x0E92


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_LIGHT_EQUIP
NAME=synthetic light effect
TYPE=T_EQ_SCRIPT
LAYER=9
ON=@Equip
SRC.NIGHTSIGHT=1
SRC.SYSMESSAGE {MARKER} <SRC.NIGHTSIGHT>|<SRC.SECTOR.LIGHT>|<SRC.ISNEARTYPE(T_LIGHT_LIT,2)>
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM SYNTHETIC_LIGHT_EQUIP
LASTNEW.ATTR=attr_decay
LASTNEW.TIMER=30
SYSMESSAGE {READY_MARKER} <LASTNEW.SERIAL>
EQUIP(<LASTNEW>)
SYSMESSAGE {MARKER}_END
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
        'TITLE="Sphere synthetic live-reference equip fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic live-reference equip fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=LightEquipReferenceProbeCharacter
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
    return 0


MODE = register_mode(
    FixtureMode(
        name="light-equip-reference",
        fixture_args=(),
        order=176,
        id_block=123,
        case=FixtureCase(
            name="light-equip-reference",
            mode="light-equip-reference",
            tests=(TestCase("test_light_equip_reference.py", (), True, True),),
            ports={"native": 3158, "asan": 3159},
            output="light-equip-reference",
        ),
    )
)
