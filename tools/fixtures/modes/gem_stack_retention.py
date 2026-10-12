"""Registered fixture for retaining a saved pile when a script adds to it."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_stackable_tile
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "GemStackRetentionProbe"
PASSWORD = "gspw"
EVENT_NAME = "e_GemStackRetentionProbe"
MARKER = "SPHERE_GEM_STACK"
ITEM_ID = 0x0EA8
PACK_SERIAL = 0x40000020
EXISTING_SERIAL = 0x40000021


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_GEM_STACK
NAME=synthetic gem stack
TYPE=T_NORMAL
WEIGHT=1
CAN=0x100

[EVENTS {EVENT_NAME}]
ON=@LogIn
VAR before,<SRC.FINDID(SYNTHETIC_GEM_STACK).SERIAL>
NEWITEM SYNTHETIC_GEM_STACK
LASTNEW.AMOUNT=5
LASTNEW.BOUNCE
SYSMESSAGE {MARKER} [<VAR(before)>|<SRC.FINDID(SYNTHETIC_GEM_STACK).SERIAL>|<SRC.FINDID(SYNTHETIC_GEM_STACK).AMOUNT>]
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
        f"""TITLE="Sphere synthetic saved pile fixture"
VERSION="0.99z8"
SAVECOUNT=0

[WorldItem 0x0E75]
SERIAL=0x{PACK_SERIAL:08X}
LAYER=21
CONT=3

[WorldItem SYNTHETIC_GEM_STACK]
SERIAL=0x{EXISTING_SERIAL:08X}
AMOUNT=1
P=50,50
CONT=0x{PACK_SERIAL:08X}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic saved pile fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=GemStackRetentionProbeCharacter
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
    write_stackable_tile(output / "muls" / "tiledata.mul", ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="gem-stack-retention",
        fixture_args=(),
        order=194,
        id_block=146,
        case=FixtureCase(
            name="gem-stack-retention",
            mode="gem-stack-retention",
            tests=(TestCase("test_gem_stack_retention.py", (), True, True),),
            ports={"native": 3210, "asan": 3211},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="gem-stack-retention",
            test_args_by_variant={},
        ),
    )
)
