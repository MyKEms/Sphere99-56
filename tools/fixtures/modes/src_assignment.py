"""Registered fixture for writable SRC and server sourced item timers."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .compat_writer import UID_F_ITEM, write_equipment_tile, write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ScriptGapsProbe"
LOGIN_VALUE = "script-gaps-pw"
EVENT_NAME = "e_SrcAssignmentProbe"
MARKER = "SPHERE_SRC_ASSIGNMENT"
TIMER_ITEM = "SYNTHETIC_SRC_TIMER"
TARGET_ITEM = "SYNTHETIC_SRC_TARGET"
CHAR_SERIAL = 3
TIMER_SERIAL = 4
TARGET_SERIAL = 5
TIMER_ITEM_ID = 0x0EA8
TARGET_ITEM_ID = 0x0EA9


def generate(output: Path) -> int:
    """Generate an existing-character fixture with timer and nested SRC rows."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # The base recipe's broad login hook creates unrelated markers. Keep the
    # common definitions and replace that hook with this mode's focused event.
    text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    text += f"""

[ITEMDEF 0x{TIMER_ITEM_ID:04X}]
DEFNAME={TIMER_ITEM}
NAME=synthetic SRC timer
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Timer
SRC=<CONT>
SERV.B {MARKER} timer_source|[<SRC.NAME>]
F_SRC_ASSIGNMENT({UID_F_ITEM | TARGET_SERIAL})
SERV.B {MARKER} timer_after|[<SRC.NAME>]
RETURN 1

[ITEMDEF 0x{TARGET_ITEM_ID:04X}]
DEFNAME={TARGET_ITEM}
NAME=synthetic SRC target
TYPE=T_NORMAL

[FUNCTION F_SRC_ASSIGNMENT]
SYSMESSAGE {MARKER} nested_before|[<SRC.NAME>]
SRC=<ARGV(0)>
SYSMESSAGE {MARKER} nested_inside|[<SRC.NAME>]
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} caller_before|[<SRC.NAME>]
SYSMESSAGE {MARKER} target_name|[<FINDUID({UID_F_ITEM | TARGET_SERIAL}).NAME>]
SYSMESSAGE {MARKER} target_serial|[<FINDUID({UID_F_ITEM | TARGET_SERIAL}).SERIAL>]
F_SRC_ASSIGNMENT({UID_F_ITEM | TARGET_SERIAL})
SYSMESSAGE {MARKER} caller_after|[<SRC.NAME>]
SYSMESSAGE {MARKER}_END
RETURN 0
ON=@Logout
RETURN 0
"""
    tables.write_text(text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
LASTCHARUID={CHAR_SERIAL}
CHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic SRC assignment fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic SRC assignment fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
NAME=SrcProbe
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDITEM {TIMER_ITEM}]
SERIAL={TIMER_SERIAL}
CONT={CHAR_SERIAL}
LAYER=30
TIMER=15
[WORLDITEM {TARGET_ITEM}]
SERIAL={TARGET_SERIAL}
CONT={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=max(TIMER_ITEM_ID, TARGET_ITEM_ID))
    write_equipment_tile(output / "muls" / "tiledata.mul", TIMER_ITEM_ID, 30)
    return 0


MODE = register_mode(
    FixtureMode(
        name="src-assignment",
        fixture_args=(),
        order=76,
        id_block=76,
        case=FixtureCase(
            name="src-assignment",
            mode="src-assignment",
            tests=(TestCase("test_src_assignment.py", (), True, True),),
            ports={"native": 2882, "asan": 2883},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="src-assignment",
            test_args_by_variant={},
        ),
    )
)
