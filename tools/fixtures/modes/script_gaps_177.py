"""Synthetic rows for the newly reached 0.99 script-reference gaps."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "Gap177Probe"
PASSWORD = "gaps177-pw"
EVENT_NAME = "e_ScriptGaps177Probe"
MARKER = "SPHERE_SCRIPT_GAPS_177"
ROOT_NAME = "SCRIPT_GAPS_177_ROOT_UID"
CHARDEF_NAME = "c_ScriptGaps177Dummy"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    if "UNKNOWNKEYWORDREPORT=" not in ini_text:
        ini.write_text(
            ini_text.replace(
                "DEBUGLEVEL=0\n",
                "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
                1,
            ),
            encoding="ascii",
        )

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    table_text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        table_text,
        count=1,
        flags=re.DOTALL,
    )
    table_text += f"""

[DEFNAMES SCRIPT_GAPS_177]
{ROOT_NAME} #40000004

[CHARDEF {CHARDEF_NAME}]
DEFNAME={CHARDEF_NAME}
ID=c_man
NAME=synthetic script-gaps 177 dummy
ON=@Create
FLAG_INSUBSTANTIAL=1
FLAG_INVUL=1
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
FINDUID(0x40000004).TAG.script_gaps_177=177
NEWNPC c_man
ACT.P=131,131,0
SYSMESSAGE {MARKER} act_after|[<ACT.P_X>]|[<ACT.P_Y>]|[<ACT.P_Z>]
SYSMESSAGE {MARKER} uid_name|[<{ROOT_NAME}.NAME>]
SYSMESSAGE {MARKER} uid_tag|[<{ROOT_NAME}.TAG.script_gaps_177>]
NEWNPC {CHARDEF_NAME}
ACT.P=132,132,0
SYSMESSAGE {MARKER} flags|[<ACT.FLAG_INSUBSTANTIAL>]|[<ACT.FLAG_INVUL>]
SYSMESSAGE {MARKER}_END
RETURN 0
ON=@Logout
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
    header = "TITLE=Sphere synthetic script-gaps 177 fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=ScriptGaps177ProbeCharacter
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDITEM DEFAULTITEM]
SERIAL=4
CONT=3
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="script-gaps-177",
        fixture_args=(),
        order=155,
        id_block=100,
        case=FixtureCase(
            name="script-gaps-177",
            mode="script-gaps-177",
            tests=(TestCase("test_script_gaps_177.py", (), True, True),),
            ports={"native": 2898, "asan": 2899},
            generator="make_fixture.py",
            output="script-gaps-177",
        ),
    )
)
