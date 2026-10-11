"""Fixture for the character ACTION property setter."""

from pathlib import Path
import re

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ActionSetterProbe"
LOGIN_VALUE = "action-setter-pw"
EVENT_NAME = "e_ActionSetterProbe"
MARKER = "SPHERE_ACTION_SETTER"


def generate(output: Path) -> int:
    """Generate a character that exercises valid and cancelling ACTION writes."""

    from .base import MODE as BASE_MODE

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    text += f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
F_ACTION_SETTER
RETURN 0
ON=@Logout
RETURN 0

[FUNCTION F_ACTION_SETTER]
// SYNTH_SKILL_0 is a fixture skill with no external spell or target state.
// ACTION's legacy getter renders skill resources as '?' on this fixture; the
// marker still proves that the value was expanded after the valid write.
SYSMESSAGE {MARKER} loaded|[<ACTION>]
ACTION=SYNTH_SKILL_0
SYSMESSAGE {MARKER} valid|[<ACTION>]
ACTION=-1
SYSMESSAGE {MARKER} minus-one|[<ACTION>]
ACTION=""
SYSMESSAGE {MARKER} empty|[<ACTION>]
ACTION=not-a-skill
ACTION=SYNTH_SKILL_0
SYSMESSAGE {MARKER}_END
SERV.SAVE 1
RETURN 0
"""
    tables.write_text(text, encoding="ascii")

    ini = output / "sphere.ini"
    ini.write_text(
        ini.read_text(encoding="ascii").replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        ),
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
CHARUID=3
CHARUID=4
CHARUID=5
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    header = "TITLE=Sphere synthetic ACTION setter fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=ActionSetterProbeCharacter
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
[WORLDCHAR c_MAN]
SERIAL=4
ACCOUNT={ACCOUNT}
NAME=ActionSetterProbeNpc
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
ACTION=SYNTH_SKILL_0
P=130,128,0
[WORLDCHAR c_MAN]
SERIAL=5
ACCOUNT={ACCOUNT}
NAME=ActionSetterNpcAction
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
ACTION=101
P=132,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="action-setter",
        fixture_args=(),
        order=192,
        id_block=142,
        case=FixtureCase(
            name="action-setter",
            mode="action-setter",
            tests=(TestCase("test_action_setter.py", (), True, True),),
            ports={"native": 3176, "asan": 3177},
            output="action-setter",
        ),
    )
)
