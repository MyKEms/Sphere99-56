"""Registered fixture for legacy single-string method arguments."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "Arg1sProbe"
LOGIN_TOKEN = "arg1s-pw"
EVENT_NAME = "e_Arg1sQuotesProbe"
MARKER = "SPHERE_ARG1S"

# This module is discovered before ``base``.  The compatibility generator only
# needs the base fixture arguments, so keep a local adapter instead of
# importing and registering the shared mode twice during discovery.
BASE_MODE = FixtureMode(name="arg1s-quotes-base", fixture_args=())


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    scripts = output / "scripts" / "spheretables.scp"
    scripts.write_text(
        scripts.read_text(encoding="ascii")
        + f"""

[FUNCTION f_Arg1sQuotesProbe]
SYSMESSAGE \"{MARKER}|verb-quoted|<EVAL 2+3>\"
SYSMESSAGE {MARKER}|verb-plain
SYSMESSAGE(\"{MARKER}|call-quoted|<EVAL 3+4>\")
SYSMESSAGE({MARKER}|call-plain)
SRC.SYSMESSAGE(\"{MARKER}|dotted-quoted|<EVAL 4+5>\")
SRC.SYSMESSAGE {MARKER}|dotted-plain
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_Arg1sQuotesProbe
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
        """TITLE=Sphere synthetic ARG1S fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic ARG1S fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=Arg1sProbe
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
        name="arg1s-quotes",
        fixture_args=(),
        order=212,
        id_block=158,
        case=FixtureCase(
            name="arg1s-quotes",
            mode="arg1s-quotes",
            tests=(TestCase("test_arg1s_quotes.py", (), True, True),),
            ports={"native": 43221, "asan": 43222},
            output="arg1s-quotes",
        ),
    )
)
