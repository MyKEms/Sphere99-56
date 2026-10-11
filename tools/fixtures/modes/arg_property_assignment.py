"""Fixture for assignments through an object held in a named ARG local."""

from pathlib import Path
import re

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ArgPropertyAssignmentProbe"
LOGIN_VALUE = "arg-pw"
EVENT_NAME = "e_ArgPropertyAssignmentProbe"
MARKER = "SPHERE_ARG_PROPERTY_ASSIGNMENT"
ITEM_NAME = "SYNTHETIC_OBJECT"


def generate(output: Path) -> int:
    """Generate a minimal existing-character assignment probe."""

    from .base import MODE as BASE_MODE

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # Keep the common definitions from the compatibility recipe, but remove
    # its broad login script so this mode reports only its own rows.
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
F_ARG_PROPERTY_ASSIGNMENT()
RETURN 0
ON=@Logout
RETURN 0

[FUNCTION F_ARG_PROPERTY_ASSIGNMENT]
ARG(itemFactory,<SRC>)
ARG(itemFactory).NEWITEM({ITEM_NAME})
ARG(testitem,<LASTNEW>)
SYSMESSAGE {MARKER} created|[<ARG(testitem).SERIAL>]
SYSMESSAGE {MARKER} before|[<ARG(testitem).P>]
ARG(testitem).P=129,128,0
ARG(testitem).NAME=ARG_ASSIGN_TEST
ARG(testitem).COLOR=0123
SYSMESSAGE {MARKER} after|[<ARG(testitem).P>]
SYSMESSAGE {MARKER} values|[<ARG(testitem).NAME>]|[<ARG(testitem).COLOR>]
SYSMESSAGE {MARKER}_END
RETURN 0
"""
    tables.write_text(text, encoding="ascii")

    ini = output / "sphere.ini"
    ini.write_text(
        ini.read_text(encoding="ascii").replace(
            "DEBUGLEVEL=0\n", "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n", 1
        ),
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    header = "TITLE=Sphere synthetic ARG property assignment fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=ArgPropertyAssignmentProbeCharacter
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
        name="arg-property-assignment",
        fixture_args=(),
        order=189,
        id_block=135,
        case=FixtureCase(
            name="arg-property-assignment",
            mode="arg-property-assignment",
            tests=(TestCase("test_arg_property_assignment.py", (), True, True),),
            ports={"native": 3170, "asan": 3171},
            output="arg-property-assignment",
        ),
    )
)
