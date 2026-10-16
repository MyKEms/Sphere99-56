"""Registered fixture for weekly quest-count calculation through a memory item."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "WeeklyQuestCountProbe"
LOGIN_PHRASE = "weekly-count-pw"
EVENT_NAME = "e_WeeklyQuestCountProbe"
MARKER = "SPHERE_WEEKLY_COUNT"
END_MARKER = MARKER + "_END"
MEMORY_NAME = "SYNTHETIC_WEEKLY_MEMORY"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES synthetic_weekly]
def_weekly_count 8
def_weekly_countBase 3
def_weekly_levelStepUp 20

[ITEMDEF 0x0E34]
DEFNAME=SYNTHETIC_MEMORY_BASE
NAME=synthetic memory base
TYPE=T_EQ_SCRIPT

[ITEMDEF {MEMORY_NAME}]
DEFNAME={MEMORY_NAME}
ID=0x0E34
TYPE=T_EQ_SCRIPT

ON=@Equip
TAG(playerLevel,<EVAL SRC.TAG(level)>)
f_weekly_setQuestAmount
RETURN 0

[FUNCTION f_weekly_setQuestAmount]
IF (<TAG(playerLevel)> >= <EVAL ((<def_weekly_count>-<def_weekly_countBase>)*<def_weekly_levelStepUp>)>)
  TAG(qCount,<def_weekly_count>)
ELSE
  TAG(qCount,<EVAL ((<TAG(playerLevel)>)/<def_weekly_levelStepUp>)+<def_weekly_countBase>>)
ENDIF
RETURN 1

[FUNCTION weeklyTag]
ARG(weekly,<FINDID({MEMORY_NAME})>)
RETURN <weekly.TAG(qCount)>

[EVENTS {EVENT_NAME}]
ON=@LogIn
TAG(level,40)
NEWEQUIP({MEMORY_NAME})
SYSMESSAGE {MARKER}|40=<weeklyTag(qCount)>
FINDID({MEMORY_NAME}).REMOVE
TAG(level,59)
NEWEQUIP({MEMORY_NAME})
SYSMESSAGE {MARKER}|59=<weeklyTag(qCount)>
SYSMESSAGE {END_MARKER}
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_PHRASE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic weekly quest count fixture\n"
        "VERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic weekly quest count fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=WeeklyQuestCountProbe
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
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="weekly-quest-count",
        fixture_args=(),
        order=216,
        id_block=162,
        case=FixtureCase(
            name="weekly-quest-count",
            mode="weekly-quest-count",
            tests=(TestCase("test_weekly_quest_count.py", (), True, True),),
            ports={"native": 45218, "asan": 45219},
            output="weekly-quest-count",
        ),
    )
)
