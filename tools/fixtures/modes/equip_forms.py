"""Registered fixture for the legacy script EQUIP argument forms."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "EquipFormsProbe"
PASSWORD = "eq-pw"
EVENT_NAME = "e_EquipFormsProbe"
ITEM_ID = 0x0E92
ITEM_DEF = "SYNTHETIC_EQUIP_FORM"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_DEF}
NAME=synthetic equip form item
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Equip
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_equip_forms_probe
RETURN 0

[FUNCTION NEWITEMSAFE]
NEWITEM <ARGV(0)>

[FUNCTION f_equip_forms_probe]
newitemsafe({ITEM_DEF})
arg(lastnew_serial,<lastnew.serial>)
equip(lastnew)
arg(lastnew_equipped,<findlayer(30).serial>)
SYSMESSAGE SPHERE_EQUIP_LASTNEW <arg(lastnew_serial)>|<arg(lastnew_equipped)>
findlayer(30).remove

newitemsafe({ITEM_DEF})
act=<lastnew.uid>
arg(act_serial,<act.serial>)
equip(act)
arg(act_equipped,<findlayer(30).serial>)
SYSMESSAGE SPHERE_EQUIP_ACT <arg(act_serial)>|<arg(act_equipped)>
findlayer(30).remove

equip(<{ITEM_DEF}>)
arg(defname_equipped,0)
if (findlayer(30))
arg(defname_equipped,<findlayer(30).serial>)
endif
SYSMESSAGE SPHERE_EQUIP_DEFNAME <arg(defname_equipped)>
findlayer(30).remove

newitemsafe({ITEM_DEF})
arg(uid_serial,<lastnew.serial>)
equip(<lastnew>)
arg(uid_equipped,<findlayer(30).serial>)
SYSMESSAGE SPHERE_EQUIP_UID <arg(uid_serial)>|<arg(uid_equipped)>
findlayer(30).remove

SYSMESSAGE SPHERE_EQUIP_FORMS_END
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
        'TITLE="Sphere synthetic EQUIP forms fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic EQUIP forms fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=EquipFormsProbeCharacter
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
        name="equip-forms",
        fixture_args=(),
        order=182,
        id_block=129,
        case=FixtureCase(
            name="equip-forms",
            mode="equip-forms",
            tests=(TestCase("test_equip_forms.py", (), True, True),),
            ports={"native": 3172, "asan": 3173},
            output="equip-forms",
        ),
    )
)
