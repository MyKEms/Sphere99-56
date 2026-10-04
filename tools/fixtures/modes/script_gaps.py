"""Registered fixture for the newly reached container and movement flags."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ScriptGapsProbe"
PW = "script-gaps-pw"
MARKER = "SPHERE_SCRIPT_GAPS"
EVENT_NAME = "e_ScriptGapsProbe"
CONTAINER_ITEM = "SYNTHETIC_GAP_CONTAINER"
CHILD_ONE = "SYNTHETIC_GAP_CHILD_ONE"
CHILD_TWO = "SYNTHETIC_GAP_CHILD_TWO"


def generate(output: Path) -> int:
    """Generate a minimal existing-character fixture on common primitives."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    # Keep the report opt-in for this focused mode.  The common recipe does
    # not enable it because most fixture cases do not inspect diagnostics.
    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    if "UNKNOWNKEYWORDREPORT=" not in ini_text:
        ini_text = ini_text.replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        )
        ini.write_text(ini_text, encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    # The common recipe installs a noisy all-player login hook.  This mode
    # owns an explicit login event so the container index starts empty and
    # the rows identify only the behavior under test.
    table_text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        table_text,
        count=1,
        flags=re.DOTALL,
    )
    tables.write_text(
        table_text
        + f"""

[ITEMDEF 0x0E93]
DEFNAME={CONTAINER_ITEM}
NAME=synthetic script gap container
TYPE=CONTAINER
TDATA2=1
ON=@FixtureFindCont
ARG(count,0)
SRC.SYSMESSAGE {MARKER} find0|[<FINDCONT(<ARG(count)>).NAME>]
ARG(count,1)
SRC.SYSMESSAGE {MARKER} find1|[<FINDCONT(<ARG(count)>).NAME>]
ARG(count,2)
SRC.SYSMESSAGE {MARKER} find2|[<FINDCONT(<ARG(count)>).NAME>]
ARG(count,0)
SRC.SYSMESSAGE {MARKER} safe0|[<SAFE FINDCONT(<ARG(count)>).RESCOUNT>]
RETURN 1

[ITEMDEF 0x0E94]
DEFNAME={CHILD_ONE}
NAME=synthetic gap child one
TYPE=CONTAINER
TDATA2=1

[ITEMDEF 0x0E95]
DEFNAME={CHILD_TWO}
NAME=synthetic gap child two
TYPE=T_NORMAL

[PROFESSION 2]
DEFNAME=FIXTURE_PROFESSION
NAME=Fixture Profession

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} flag_initial|[<FLAG_IMMOBILE>]
FLAG_IMMOBILE=1
SYSMESSAGE {MARKER} flag_property|[<FLAG_IMMOBILE>]
FLAG_IMMOBILE(0)
SYSMESSAGE {MARKER} flag_method|[<FLAG_IMMOBILE>]
SYSMESSAGE {MARKER} trigger|[<FINDUID(0x40000004).TRIGGER(@FixtureFindCont)>]
safe(profession=FIXTURE_PROFESSION)
SYSMESSAGE {MARKER} profession_ref|[<profession>]
SYSMESSAGE {MARKER} profession_name|[<profession.name>]
SYSMESSAGE {MARKER} profession_cmp|[<eval profession==FIXTURE_PROFESSION>]
SYSMESSAGE {MARKER} setup
RETURN 0
ON=@Logout
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PW}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    # Both files of a save carry the same SAVECOUNT header; the server
    # rejects a pair in which only the character file has one.
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic script gaps fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic script gaps fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
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
[WORLDITEM {CONTAINER_ITEM}]
SERIAL=4
CONT=3
[WORLDITEM {CHILD_TWO}]
SERIAL=5
CONT=0x40000004
[WORLDITEM {CHILD_ONE}]
SERIAL=6
CONT=0x40000004
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="script-gaps",
        # The custom generator owns the recipe, but an empty argument tuple
        # keeps this mode selectable through the manifest dispatcher.
        fixture_args=(),
        order=73,
        id_block=73,
        case=FixtureCase(
            name="script-gaps",
            mode="script-gaps",
            tests=(TestCase("test_script_gaps.py", (), True, True),),
            ports={"native": 2880, "asan": 2881},
            generator="make_fixture.py",
            output="script-gaps",
        ),
    )
)
