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
TYPE=T_EQ_MEMORY_OBJ
LAYER=LAYER_SPECIAL

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

[FUNCTION weeklyGenerate]
NEWEQUIP({MEMORY_NAME})

[FUNCTION weeklyProbe]
TAG(level,<ARGV(0)>)
weeklyGenerate
IF (<weeklyTag(qCount)> != <ARGV(1)>)
  SYSMESSAGE {MARKER}|<ARGV(0)>=<weeklyTag(qCount)>
ELSE
  SYSMESSAGE {MARKER}|<ARGV(0)>=<weeklyTag(qCount)>
ENDIF
FINDID({MEMORY_NAME}).REMOVE

[FUNCTION weeklyTag]
ARG(weekly,<FINDID({MEMORY_NAME})>)
IF (<ARGVCOUNT> >= 2)
  weekly.TAG(<ARGV(0)>,"<ARGV(1)>")
  RETURN""
ELSEIF (<ARGVCOUNT> == 1)
  IF !(<HVAL ARG(weekly)>)
    RETURN""
  ENDIF
  IF (STRLEN(<weekly.TAG(<ARGV(0)>)>))
    RETURN<weekly.TAG(<ARGV(0)>)>
  ELSE
    RETURN""
  ENDIF
ENDIF
RETURN""

[EVENTS {EVENT_NAME}]
ON=@LogIn
weeklyProbe(40,5)
weeklyProbe(59,5)
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
