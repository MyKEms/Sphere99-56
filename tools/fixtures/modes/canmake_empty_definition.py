"""Synthetic coverage for CANMAKE eligibility with empty make rules."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "CanMakeProbe"
PASSWORD = "canmake-pw"
EVENT_NAME = "e_CanMakeEligibilityProbe"
MARKER = "SPHERE_CANMAKE_ELIGIBILITY"
EMPTY_ITEM_ID = 0x0F91
RULE_ITEM_ID = 0x0F92


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{EMPTY_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_CANMAKE_EMPTY
NAME=synthetic item without make rules
TYPE=T_NORMAL

[ITEMDEF 0x{RULE_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_CANMAKE_RULE
NAME=synthetic item with a make rule
TYPE=T_NORMAL
SKILLMAKE=SYNTH_SKILL_0 0.0

[FUNCTION f_CanMakeEligibilityProbe]
SYSMESSAGE {MARKER}|empty=<CANMAKE(SYNTHETIC_CANMAKE_EMPTY)>
SYSMESSAGE {MARKER}|rule=<CANMAKE(SYNTHETIC_CANMAKE_RULE)>
SYSMESSAGE {MARKER}|done
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_CanMakeEligibilityProbe
RETURN 0
""",
        encoding="ascii",
    )
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
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic CANMAKE eligibility fixture\n"
        "VERSION=0.99z8\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic CANMAKE eligibility fixture
VERSION=0.99z8
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
    write_mul_fixture(output, extra_item_id=max(EMPTY_ITEM_ID, RULE_ITEM_ID))
    return 0


MODE = register_mode(
    FixtureMode(
        name="canmake-empty-definition",
        fixture_args=(),
        order=205,
        id_block=148,
        case=FixtureCase(
            name="canmake-empty-definition",
            mode="canmake-empty-definition",
            tests=(TestCase("test_canmake_empty_definition.py", (), True, True),),
            ports={"native": 3210, "asan": 3211},
            output="canmake-empty-definition",
        ),
    )
)
