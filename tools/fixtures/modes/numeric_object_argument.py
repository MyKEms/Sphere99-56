"""Synthetic coverage for numeric conversion of dotted object results."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NidArgProbe"
LOGIN_TOKEN = "nid-arg-pw"
EVENT_NAME = "e_NumericObjectArgumentProbe"
MARKER = "SPHERE_NUMERIC_OBJECT_ARGUMENT"
ITEM_ID = 0x0F90
VALUE = 0x1234


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES numeric_object_argument_defs]
SYNTHETIC_NID_VALUE 0x{VALUE:04x}
SYNTHETIC_NID_RESOURCE 0x{ITEM_ID:04x}

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_NID_ITEM
NAME=synthetic numeric argument item
TYPE=T_NORMAL

[FUNCTION f_numeric_object_argument_probe]
NEWITEM SYNTHETIC_NID_ITEM
ARG(item,<LASTNEW>)
SYSMESSAGE {MARKER}|literal=<NID(0x1234)>
SYSMESSAGE {MARKER}|defname=<NID(SYNTHETIC_NID_VALUE)>
SYSMESSAGE {MARKER}|resource=<NID(SYNTHETIC_NID_RESOURCE)>
SYSMESSAGE {MARKER}|dotted=<NID(ARG(item).DISPIDDEC)>
SYSMESSAGE {MARKER}|lastnew=<NID(LASTNEW.DISPIDDEC)>
SYSMESSAGE {MARKER}|done
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_numeric_object_argument_probe
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_TOKEN}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic numeric object argument fixture\n"
        "VERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic numeric object argument fixture
VERSION=0.99
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

    from modes.compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="numeric-object-argument",
        fixture_args=(),
        order=196,
        id_block=147,
        case=FixtureCase(
            name="numeric-object-argument",
            mode="numeric-object-argument",
            tests=(TestCase("test_numeric_object_argument.py", (), True, True),),
            ports={"native": 43201, "asan": 43202},
            output="numeric-object-argument",
        ),
    )
)
