"""Registered synthetic fixture for the legacy NEWLOOT template command."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NewLootProbe"
PASSWORD = "newloot-pw"
EVENT_NAME = "e_NewLootProbe"
MARKER = "SPHERE_NEWLOOT"

# Keep the display IDs outside the IDs used by the shared compatibility writer.
SHIRT_ID = 0x0EB0
WEAPON_ID = 0x0EB1
PACK_ID = 0x0EB2

SHIRT_NAME = "SYNTHETIC_NEWLOOT_SHIRT"
WEAPON_NAME = "SYNTHETIC_NEWLOOT_WEAPON"
PACK_NAME = "SYNTHETIC_NEWLOOT_PACK"
TEMPLATE_NAME = "SYNTHETIC_NEWLOOT_TEMPLATE"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    base_tables = tables.read_text(encoding="ascii")
    base_tables = base_tables.replace(
        "[CHARDEF 0x0190]\n", "[CHARDEF 0x0190]\nCAN=0x314\n", 1
    )
    tables.write_text(
        base_tables
        + f"""

[ITEMDEF 0x{SHIRT_ID:04X}]
DEFNAME={SHIRT_NAME}
NAME=synthetic newloot shirt
TYPE=T_CLOTHING
LAYER=5

[ITEMDEF 0x{WEAPON_ID:04X}]
DEFNAME={WEAPON_NAME}
NAME=synthetic newloot weapon
TYPE=T_WEAPON_SWORD
LAYER=1
REQSTR=1

[ITEMDEF 0x{PACK_ID:04X}]
DEFNAME={PACK_NAME}
NAME=synthetic newloot pack item
TYPE=T_NORMAL

[TYPEDEF 13]
DEFNAME=T_WEAPON_SWORD

[TEMPLATE {TEMPLATE_NAME}]
ITEM={SHIRT_NAME}
ITEM={WEAPON_NAME}
ITEM={PACK_NAME},3
COLOR=0x0456

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWLOOT {TEMPLATE_NAME}
SYSMESSAGE {MARKER}_EQUIP <SRC.FINDLAYER(5).SERIAL>|<SRC.FINDLAYER(1).SERIAL>
SYSMESSAGE {MARKER}_PACK <SRC.FINDID({PACK_NAME}).SERIAL>|<SRC.FINDID({PACK_NAME}).CONT.SERIAL>|<SRC.FINDID({PACK_NAME}).AMOUNT>|<SRC.FINDID({PACK_NAME}).COLOR>
SYSMESSAGE {MARKER}_LASTNEW <LASTNEW.SERIAL>|<LASTNEW.AMOUNT>
SYSMESSAGE {MARKER}_END
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
        "TITLE=Sphere synthetic NEWLOOT fixture\nVERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic NEWLOOT fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=NewLootProbe
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )

    # The fixture MUL set needs visible equipment records for both equipped
    # definitions.  Keep the synthetic resources within the shared tiledata.
    from .compat_writer import write_equipment_tile, write_mul_fixture

    write_mul_fixture(output, extra_item_id=max(SHIRT_ID, WEAPON_ID, PACK_ID))
    write_equipment_tile(output / "muls" / "tiledata.mul", SHIRT_ID, 5)
    write_equipment_tile(output / "muls" / "tiledata.mul", WEAPON_ID, 1)
    return 0


MODE = register_mode(
    FixtureMode(
        name="newloot",
        fixture_args=(),
        order=158,
        id_block=103,
        case=FixtureCase(
            name="newloot",
            mode="newloot",
            tests=(TestCase("test_newloot.py", (), True, True),),
            ports={"native": 3130, "asan": 3131},
            output="newloot",
        ),
    )
)
