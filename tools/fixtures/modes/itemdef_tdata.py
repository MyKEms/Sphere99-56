"""Registered synthetic fixture for resource names in item definition TDATA."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ItemDefTDataProbe"
LOGIN_TOKEN = "itemdef-tdata-pw"
EVENT_NAME = "e_ItemDefTDataProbe"
MARKER = "SPHERE_ITEMDEF_TDATA"

BOTTLE_NAME = "synthetic empty bottle"


def _probe_lines() -> str:
    return f"""NEWITEM i_TDataProbePotion
SYSMESSAGE {MARKER} tdata1 [<LASTNEW.TYPEDEF.TDATA1>] tdata2 [<LASTNEW.TYPEDEF.TDATA2>] tdata3 [<LASTNEW.TYPEDEF.TDATA3>] tdata4 [<LASTNEW.TYPEDEF.TDATA4>]
ARG(tdata_named,<HVAL <LASTNEW.TYPEDEF.TDATA1>>)
ARG(tdata_numeric,<HVAL <LASTNEW.TYPEDEF.TDATA2>>)
LASTNEW.REMOVE
NEWITEM <ARG(tdata_named)>
SYSMESSAGE {MARKER} named [<LASTNEW.NAME>]
LASTNEW.REMOVE
NEWITEM <ARG(tdata_numeric)>
SYSMESSAGE {MARKER} numeric [<LASTNEW.NAME>]
LASTNEW.REMOVE
SYSMESSAGE {MARKER} done"""


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0f0e]
DEFNAME=i_TDataProbeBottle
NAME={BOTTLE_NAME}

[ITEMDEF i_TDataProbePotion]
NAME=synthetic potion
ID=i_TDataProbeBottle
TDATA1=i_TDataProbeBottle
TDATA2=0f0e
TDATA3=c_man
TDATA4=5

[EVENTS {EVENT_NAME}]
ON=@LogIn
{_probe_lines()}
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
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic item TDATA fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic item TDATA fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ItemDefTDataProbe
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
    return 0


MODE = register_mode(
    FixtureMode(
        name="itemdef-tdata",
        fixture_args=(),
        order=154,
        id_block=99,
        case=FixtureCase(
            name="itemdef-tdata",
            mode="itemdef-tdata",
            tests=(TestCase("test_itemdef_tdata.py", (), True, True),),
            ports={"native": 2998, "asan": 2999},
            output="itemdef-tdata",
        ),
    )
)
