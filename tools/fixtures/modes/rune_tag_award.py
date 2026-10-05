"""Registered synthetic fixture for a once-only award kept in an equipped item's TAG."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "RuneTagProbe"
LOGIN_TOKEN = "rune-tag-pw"
EVENT_NAME = "e_RuneTagProbe"
MARKER = "SPHERE_RUNE_TAG"
RUNE_ID = 0x1F14

# (label, expected value).  A script keeps "already awarded" in a TAG of a
# hidden equipped item, reached through an ARG local that holds the item.  The
# first call awards and records; the second call must find the record.
ROWS = (
    ("award1", "give"),
    ("rune", "yes"),
    ("direct", "1"),
    ("read", "1"),
    ("award2", "skip"),
)


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{RUNE_ID:04X}]
DEFNAME=i_RuneTagProbe
NAME=synthetic record rune
TYPE=T_EQ_SCRIPT
LAYER=30

[FUNCTION f_RuneTag]
IF !(ISCHAR)
 RETURN ""
ENDIF
ARG(MyRune,<FINDID(i_RuneTagProbe)>)
IF (<ARGVCOUNT> >= 2)
 IF !(<HVAL ARG(MyRune)>)
  NEWEQUIP i_RuneTagProbe
  ARG(MyRune,<LASTNEW>)
 ENDIF
 MyRune.TAG(<ARGV(0)>,"<ARGV(1)>")
 RETURN ""
ELIF (<ARGVCOUNT> == 1)
 IF !(<FINDID(i_RuneTagProbe)>)
  RETURN ""
 ENDIF
 IF (STRLEN(<MyRune.TAG(<ARGV(0)>)>))
  RETURN <MyRune.TAG(<ARGV(0)>)>
 ELSE
  RETURN ""
 ENDIF
ENDIF
RETURN ""

[FUNCTION f_RuneTagAward]
IF (0<f_RuneTag(<ARGV(0)>)>)
 SYSMESSAGE {MARKER} <ARGV(1)>=[skip]
 RETURN
ENDIF
SYSMESSAGE {MARKER} <ARGV(1)>=[give]
f_RuneTag(<ARGV(0)>,1)

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_RuneTagAward(probe_action,award1)
IF (<FINDID(i_RuneTagProbe)>)
 SYSMESSAGE {MARKER} rune=[yes]
ELSE
 SYSMESSAGE {MARKER} rune=[no]
ENDIF
SYSMESSAGE {MARKER} direct=[<FINDID(i_RuneTagProbe).TAG(probe_action)>]
SYSMESSAGE {MARKER} read=[<f_RuneTag(probe_action)>]
f_RuneTagAward(probe_action,award2)
SYSMESSAGE {MARKER} done
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
        """TITLE=Sphere synthetic rune-tag fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic rune-tag fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=RuneTagProbe
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
        name="rune-tag-award",
        fixture_args=(),
        order=166,
        id_block=111,
        case=FixtureCase(
            name="rune-tag-award",
            mode="rune-tag-award",
            tests=(TestCase("test_rune_tag_award.py", (), True, True),),
            ports={"native": 2978, "asan": 2979},
            output="rune-tag-award",
        ),
    )
)
