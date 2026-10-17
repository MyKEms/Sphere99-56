"""Synthetic coverage for one client equip request and its update packet."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_equipment_tile, write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "EquipPacketProbe"
LOGIN_CODE = "equip-packet-pw"
EVENT_NAME = "e_EquipPacketProbe"
MARKER = "SPHERE_EQUIP_PACKET"
ITEM_ID = 0x0EA3
ITEM_NAME = "SYNTHETIC_EQUIP_BLADE"
SELF_REMOVE_ID = 0x0EA4
SELF_REMOVE_NAME = "SYNTHETIC_EQUIP_SELF_REMOVE"
ARCHERY_MARKER = "SPHERE_EQUIP_ARCHERY"
ARCHERY_ONE_HAND_ID = 0x0EF0
ARCHERY_ONE_HAND_NAME = "SYNTHETIC_ARCHERY_ONE_HAND"
ARCHERY_SHIELD_ID = 0x0EF1
ARCHERY_SHIELD_NAME = "SYNTHETIC_ARCHERY_SHIELD"
ARCHERY_BOW_ID = 0x0EF2
ARCHERY_BOW_NAME = "SYNTHETIC_ARCHERY_BOW"
ARCHERY_DRIVER_ID = 0x0EF3
ARCHERY_DRIVER_NAME = "SYNTHETIC_ARCHERY_DRIVER"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    table_text = table_text.replace(
        "[CHARDEF 0x0190]\n", "[CHARDEF 0x0190]\nCAN=0x314\n", 1
    )
    tables.write_text(
        table_text
        + f"""

[TYPEDEF 14]
DEFNAME=T_WEAPON_FENCE

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic equip blade
TYPE=T_WEAPON_FENCE
LAYER=1
REQSTR=1
DAM=20
SKILL=FENCING

[ITEMDEF 0x{SELF_REMOVE_ID:04X}]
DEFNAME={SELF_REMOVE_NAME}
NAME=synthetic self-removing equip blade
TYPE=T_WEAPON_FENCE
LAYER=1
REQSTR=1
DAM=20
SKILL=FENCING
ON=@Equip
REMOVE
RETURN 1

[ITEMDEF 0x{ARCHERY_ONE_HAND_ID:04X}]
DEFNAME={ARCHERY_ONE_HAND_NAME}
NAME=synthetic one hand weapon
TYPE=14
LAYER=1
TWOHANDS=0
REQSTR=1
DAM=20
ON=@ItemEquip
ACT.UPDATE
RETURN 0

[ITEMDEF 0x{ARCHERY_SHIELD_ID:04X}]
DEFNAME={ARCHERY_SHIELD_NAME}
NAME=synthetic shield
TYPE=180
LAYER=2
REQSTR=1
ARMOR=5
ON=@ItemEquip
ACT.UPDATE
RETURN 0

[ITEMDEF 0x{ARCHERY_BOW_ID:04X}]
DEFNAME={ARCHERY_BOW_NAME}
NAME=synthetic two hand bow
TYPE=15
LAYER=2
TWOHANDS=1
REQSTR=1
DAM=20
ON=@ItemEquip
ACT.UPDATE
RETURN 0

[ITEMDEF 0x{ARCHERY_DRIVER_ID:04X}]
DEFNAME={ARCHERY_DRIVER_NAME}
NAME=synthetic archery driver
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Equip
TIMER=1
RETURN 0
ON=@Timer
CONT.NEWEQUIP {ARCHERY_ONE_HAND_NAME}
CONT.TAG.ARCHERY_ONE=<LASTNEW.SERIAL>
CONT.NEWEQUIP {ARCHERY_SHIELD_NAME}
CONT.TAG.ARCHERY_SHIELD=<LASTNEW.SERIAL>
CONT.NEWEQUIP {ARCHERY_BOW_NAME}
CONT.TAG.ARCHERY_BOW=<LASTNEW.SERIAL>
CONT.SYSMESSAGE {ARCHERY_MARKER}|<CONT.TAG.ARCHERY_ONE>|<CONT.TAG.ARCHERY_SHIELD>|<CONT.TAG.ARCHERY_BOW>|<CONT.FINDLAYER(1).SERIAL>|<CONT.FINDLAYER(2).SERIAL>
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}_READY|<SRC.FINDID({ITEM_NAME}).SERIAL>|<SRC.SERIAL>|<SRC.FINDLAYER(21).SERIAL>|<SRC.FINDID({SELF_REMOVE_NAME}).SERIAL>
SRC.NEWITEM={ARCHERY_DRIVER_NAME}
EQUIP(<LASTNEW>)
RETURN 0
ON=@ItemEquip
ACT.UPDATE
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_CODE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic equip packet fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic equip packet fixture"
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
P=128,128,0
[WORLDITEM 0x0E75]
SERIAL=4
LAYER=21
CONT=3
[WORLDITEM {ITEM_NAME}]
SERIAL=5
AMOUNT=1
P=40,40
CONT=0x40000004
[WORLDITEM {SELF_REMOVE_NAME}]
SERIAL=6
AMOUNT=1
P=41,40
CONT=0x40000004
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    write_equipment_tile(output / "muls" / "tiledata.mul", ITEM_ID, 1)
    write_equipment_tile(output / "muls" / "tiledata.mul", SELF_REMOVE_ID, 1)
    write_equipment_tile(output / "muls" / "tiledata.mul", ARCHERY_ONE_HAND_ID, 1)
    write_equipment_tile(output / "muls" / "tiledata.mul", ARCHERY_SHIELD_ID, 2)
    write_equipment_tile(output / "muls" / "tiledata.mul", ARCHERY_BOW_ID, 2)
    write_equipment_tile(output / "muls" / "tiledata.mul", ARCHERY_DRIVER_ID, 30)
    return 0


MODE = register_mode(
    FixtureMode(
        name="equip-packet",
        fixture_args=(),
        order=218,
        id_block=164,
        case=FixtureCase(
            name="equip-packet",
            mode="equip-packet",
            tests=(TestCase("test_equip_packet.py", (), True, True),),
            ports={"native": 45222, "asan": 45223},
            output="equip-packet",
        ),
    )
)
