"""Synthetic coverage for unresolved item names in script item paths."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "UndefinedItemProbe"
LOGIN_VALUE = "undef-item-pw"
EVENT_NAME = "e_UndefinedItemProbe"
MARKER = "SPHERE_UNDEFINED_ITEM"
UNKNOWN_NAME = "SYNTHETIC_UNKNOWN_ITEM"
ITEM_ID = 0x0EA0


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_DEFINED_ITEM
NAME=synthetic defined item
TYPE=T_NORMAL

[EVENTS {EVENT_NAME}]
ON=@LogIn
IF <TAG.probe_done>
RETURN 0
ENDIF
TAG.probe_done=1
SYSMESSAGE {MARKER}|can=<CANMAKE({UNKNOWN_NAME})>
LASTNEW.REMOVE
NEWITEM {UNKNOWN_NAME}
SYSMESSAGE {MARKER}|new=<LASTNEW.SERIAL>
LASTNEW.REMOVE
NEWITEM SYNTHETIC_DEFINED_ITEM
SYSMESSAGE {MARKER}|defined=<LASTNEW.SERIAL>
LASTNEW.REMOVE
SYSMESSAGE {MARKER}|end
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
        'TITLE="Sphere synthetic unresolved item fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic unresolved item fixture"
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
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="undefined-item",
        fixture_args=(),
        order=214,
        id_block=160,
        case=FixtureCase(
            name="undefined-item",
            mode="undefined-item",
            tests=(TestCase("test_undefined_item.py", (), True, True),),
            ports={"native": 45216, "asan": 45217},
            output="undefined-item",
        ),
    )
)
