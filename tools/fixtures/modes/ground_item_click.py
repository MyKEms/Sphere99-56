"""Registered synthetic fixture for ground-item trigger references."""

from __future__ import annotations

from pathlib import Path
import re

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode
from .base import MODE as BASE_MODE


ACCOUNT = "GroundProbe"
LOGIN_VALUE = "ground-item-pw"
CHAR_SERIAL = 3
SAME_Z_SERIAL = 100
FAR_Z_SERIAL = 101
FAR_ITEM_DEFNAME = "SYNTHETIC_GROUND_FAR"
SAME_Z_UID = 0x40000000 | SAME_Z_SERIAL
EVENT_NAME = "e_GroundItemClick"
MARKER = "SPHERE_GROUND"


def generate(output: Path) -> int:
    """Generate one player and two movable ground items."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    table_text, count = re.subn(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        table_text,
        count=1,
        flags=re.DOTALL,
    )
    if count != 1:
        raise RuntimeError("base fixture login event was not found exactly once")
    tables.write_text(
        table_text
        + f"""

[FUNCTION absolute]
IF (<ARGV(0)> < 0)
  RETURN <EVAL 0-<ARGV(0)>>
ENDIF
RETURN <ARGV(0)>

[FUNCTION f_noclickhigh]
IF (<absolute(<EVAL (<ARGV(0)>-<ARGV(1)>)>)> >= 20)
  RETURN 1
ENDIF
RETURN 0

[FUNCTION itemExists]
IF (<ARGV(0)> == 0)
  RETURN 0
ENDIF
IF (safe finduid(<ARGV(0)>).isItem)
  RETURN 1
ENDIF
RETURN 0

[ITEMDEF 0x0E87]
DEFNAME={FAR_ITEM_DEFNAME}
NAME=synthetic ground far marker
TYPE=T_SIGN_GUMP

[EVENTS {EVENT_NAME}]
ON=@LogIn
FINDUID(0x40000065).TRIGGER(@UserDClick)
RETURN 0
ON=@ItemUserDClick
IF (<UID> != <ACT.TOPOBJ>)
  IF (<f_noclickhigh(<P_Z>,<ACT.TOPOBJ.P_Z>)>)
    SYSMESSAGE {MARKER}_TOO_FAR
    RETURN 1
  ENDIF
ENDIF
SYSMESSAGE {MARKER}_USED|<ACT>|<ACT.SERIAL>|<ACT.P_Z>|<P_Z>
RETURN 0
ON=@ItemDropon_Ground
SYSMESSAGE {MARKER}_DROP|<itemExists(<ACT>)>|<ACT>|<ACT.SERIAL>|<ACT.P_Z>
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
PLEVEL=Player
LASTCHARUID={CHAR_SERIAL}
CHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic ground-item trigger fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM SYNTHETIC_OBJECT]
SERIAL={SAME_Z_SERIAL}
ATTR=MOVE_ALWAYS
P=128,128,10
[WORLDITEM {FAR_ITEM_DEFNAME}]
SERIAL={FAR_Z_SERIAL}
ATTR=MOVE_ALWAYS
P=128,128,30
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic ground-item trigger fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME=GroundItemClickProbe
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,10
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="ground-item-click",
        fixture_args=(),
        order=97,
        id_block=97,
        case=FixtureCase(
            name="ground-item-click",
            mode="ground-item-click",
            tests=(TestCase("test_ground_item_click.py", (), True, True),),
            ports={"native": 2900, "asan": 2901},
            output="ground-item-click",
        ),
    )
)
