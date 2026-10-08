"""Registered fixture for the stock NEWITEM placement cleanup contract."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "UnplacedNewItemProbe"
PASSWORD = "unplaced-pw"
EVENT_NAME = "e_UnplacedNewItemProbe"
MARKER = "SPHERE_UNPLACED_NEWITEM"
UNPLACED_ID = 0x0EB3
PLACED_ID = 0x0EB4
UNPLACED_NAME = "SYNTHETIC_UNPLACED_PROBE"
PLACED_NAME = "SYNTHETIC_PLACED_PROBE"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    # The base recipe deliberately creates a disposable unplaced hair item for
    # its broad compatibility checks.  This mode owns the two-item contract,
    # so remove that unrelated item to keep the cleanup count deterministic.
    table_text = table_text.replace("NEWITEM SYNTHETIC_HAIR\n", "", 1)
    table_text += f"""

[ITEMDEF 0x{UNPLACED_ID:04X}]
DEFNAME={UNPLACED_NAME}
NAME=synthetic unplaced probe
TYPE=T_NORMAL

[ITEMDEF 0x{PLACED_ID:04X}]
DEFNAME={PLACED_NAME}
NAME=synthetic placed probe
TYPE=T_NORMAL

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {UNPLACED_NAME}
TAG.unplaced=<LASTNEW.SERIAL>
SYSMESSAGE {MARKER}_CREATED <LASTNEW.SERIAL>|<LASTNEW.P>
NEWITEM {PLACED_NAME}
LASTNEW.P=128,128,0
TAG.placed=<LASTNEW.SERIAL>
SYSMESSAGE {MARKER}_PLACED <LASTNEW.SERIAL>|<LASTNEW.P>
SYSMESSAGE {MARKER}_LOOKUP <FINDUID(<TAG.unplaced>).NAME>|<FINDUID(<TAG.placed>).NAME>
SERV.SAVE 1
SYSMESSAGE {MARKER}_END
RETURN 0
"""
    tables.write_text(table_text, encoding="ascii")

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
        "TITLE=Sphere synthetic unplaced NEWITEM fixture\n"
        "VERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic unplaced NEWITEM fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=UnplacedNewItemProbe
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

    from .compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=PLACED_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="unplaced-newitem",
        fixture_args=(),
        order=177,
        id_block=124,
        case=FixtureCase(
            name="unplaced-newitem",
            mode="unplaced-newitem",
            tests=(TestCase("test_unplaced_newitem.py", (), True, True),),
            ports={"native": 3160, "asan": 3161},
            output="unplaced-newitem",
        ),
    )
)
