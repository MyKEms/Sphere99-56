"""Synthetic rows for hash-serial FINDUID, CONT arguments, and flags."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "FindUidFlags179Probe"
PASSWORD = "fuf179-pw"
EVENT_NAME = "e_FindUidFlags179Probe"
MARKER = "SPHERE_FINDUID_FLAGS_179"
ITEM_ROOT_ID = 0x0EAC


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    table_text += f"""

[TYPEDEFS]
T_CONTAINER 1

[ITEMDEF 0x{ITEM_ROOT_ID:04X}]
DEFNAME=I_FINDUID_FLAGS_179_TARGET
NAME=synthetic hash target
TYPE=T_CONTAINER

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} hash_name|[<FINDUID(#00000004).NAME>]
SYSMESSAGE {MARKER} hex_name|[<FINDUID(0x00000004).NAME>]
f_cont_fixture <FINDUID(0x00000004)>
SYSMESSAGE {MARKER} flags_before|[<FLAG_FREEZE>]|[<FLAG_STONE>]|[<STONE>]
FLAG_FREEZE=1
FLAG_STONE=1
SYSMESSAGE {MARKER} flags_after|[<FLAG_FREEZE>]|[<FLAG_STONE>]|[<STONE>]
FLAG_FREEZE=0
FLAG_STONE=0
SYSMESSAGE {MARKER} flags_reset|[<FLAG_FREEZE>]|[<FLAG_STONE>]|[<STONE>]
SYSMESSAGE {MARKER}_END
RETURN 0
ON=@Logout
RETURN 0

[FUNCTION f_cont_fixture]
ARG(moveto,<FINDUID(args)>)
SYSMESSAGE {MARKER} arg_value|[<ARG(moveto)>]|[<ARG(moveto).NAME>]
NEWITEM 0x{ITEM_ROOT_ID:04X}
LASTNEW.NAME=synthetic hash child
LASTNEW.CONT=<ARG(moveto)>
SYSMESSAGE {MARKER} cont_name|[<LASTNEW.CONT.NAME>]
RETURN 0
"""
    tables.write_text(table_text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    header = "TITLE=Sphere synthetic FINDUID flags 179 fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=FindUidFlags179ProbeCharacter
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDITEM I_FINDUID_FLAGS_179_TARGET]
SERIAL=4
CONT=3
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="finduid-flags-179",
        fixture_args=(),
        order=156,
        id_block=101,
        case=FixtureCase(
            name="finduid-flags-179",
            mode="finduid-flags-179",
            tests=(TestCase("test_finduid_flags_179.py", (), True, True),),
            ports={"native": 2900, "asan": 2901},
            generator="make_fixture.py",
            output="finduid-flags-179",
        ),
    )
)
