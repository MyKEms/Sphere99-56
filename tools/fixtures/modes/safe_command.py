"""Registered fixture for SAFE-wrapped command side effects."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "SafeCommandProbe"
PASSWORD = "sc-pw"
EVENT_NAME = "e_SafeCommandProbe"
ITEM_ID = 0xA5E0
ITEM_NAME = "SYNTHETIC_SAFE_COMMAND_ITEM"
MACRO_ITEM_ID = 0xA5E1
MACRO_ITEM_NAME = "SYNTHETIC_SAFE_COMMAND_MACRO_ITEM"
MARKER = "SPHERE_SAFE_COMMAND"
MACRO_MARKER = MARKER + " MACRO_CALLBACK"
MEMORY_MARKER = MARKER + " MEMORY_CALLBACK"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic safe command item
TYPE=T_EQ_SCRIPT
LAYER=77
ON=@Equip
CONT.SYSMESSAGE {MARKER} CALLBACK
REMOVE

[ITEMDEF 0x{MACRO_ITEM_ID:04X}]
DEFNAME={MACRO_ITEM_NAME}
NAME=synthetic safe command macro item
TYPE=T_EQ_SCRIPT
LAYER=77
ON=@Equip
CONT.SYSMESSAGE {MACRO_MARKER}
REMOVE

[ITEMDEF i_cansee]
DEFNAME=i_cansee
ID=0x2007
NAME=synthetic can-see memory
TYPE=T_SRC_CHANGER
LAYER=77
ON=@Equip
CONT.SYSMESSAGE {MEMORY_MARKER}
REMOVE

[EVENTS {EVENT_NAME}]
ON=@LogIn
IF <TAG.probe_done>
RETURN 0
ENDIF
TAG.probe_done=1
f_safe_command_probe({ITEM_NAME})
SYSMESSAGE {MARKER} END
RETURN 0

[FUNCTION f_safe_command_probe]
f_safe_newitem(<ARGS>)
LASTNEW.TAG(probe,1)
SAFE EQUIP(LASTNEW)
NEWITEM 0x{MACRO_ITEM_ID:04X}
LASTNEW.LINK=<SRC>
SAFE EQUIP(<LASTNEW>)
f_safe_memory_probe(<SRC>)
RACEMESSAGE("{MARKER} MESSAGE")
RETURN 0

[FUNCTION f_safe_memory_probe]
NEWITEMSAFE(i_cansee)
LASTNEW.LINK=<ARGS>
SAFE EQUIP(<LASTNEW>)
RETURN 0

[FUNCTION NEWITEMSAFE]
NEWITEM <ARGS>
RETURN <LASTNEW.SERIAL>

[FUNCTION f_safe_newitem]
ARG(saved_act,<ACT>)
NEWITEM(<HVAL ARGV(0)>)
ACT=<ARG(saved_act)>
RETURN 0

[FUNCTION RACEMESSAGE]
SYSMESSAGE <ARGS>
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
        'TITLE="Sphere synthetic SAFE command fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic SAFE command fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=SafeCommandProbeCharacter
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

    write_mul_fixture(output, extra_item_id=MACRO_ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="safe-command",
        fixture_args=(),
        order=209,
        id_block=155,
        case=FixtureCase(
            name="safe-command",
            mode="safe-command",
            tests=(TestCase("test_safe_command.py", (), True, True),),
            ports={"native": 43215, "asan": 43216},
            output="safe-command",
        ),
    )
)
