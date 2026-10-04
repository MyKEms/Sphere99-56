"""Registered fixture for bare zero-argument function predicates."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "BareFnProbe"
LOGIN_TOKEN = "bare-fn-pw"
EVENT_NAME = "e_BareFunctionPredicateProbe"
MARKER = "SPHERE_BARE_FUNCTION_PREDICATE"

# This module sorts before ``base`` during discovery, so importing and
# registering the shared base mode here would register it twice.  The legacy
# writer only needs its argument selection; keep this private adapter local.
BASE_MODE = FixtureMode(name="bare-function-predicate-base", fixture_args=())


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[FUNCTION f_BareFunctionPredicateSide]
VAR bare_function_predicate_calls,<EVAL <VAR(bare_function_predicate_calls)>+1>
RETURN 1

[FUNCTION f_BareFunctionPredicateControl]
VAR bare_function_predicate_calls,0
IF (f_BareFunctionPredicateSide)
  SYSMESSAGE {MARKER} result=true
ELSE
  SYSMESSAGE {MARKER} result=false
ENDIF
SYSMESSAGE {MARKER} calls=[<VAR(bare_function_predicate_calls)>]
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_BareFunctionPredicateControl
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
        """TITLE=Sphere synthetic bare function predicate fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic bare function predicate fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=BareFunctionPredicateProbe
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
        name="bare-function-predicate",
        fixture_args=(),
        order=155,
        id_block=100,
        case=FixtureCase(
            name="bare-function-predicate",
            mode="bare-function-predicate",
            tests=(TestCase("test_bare_function_predicate.py", (), True, True),),
            ports={"native": 3128, "asan": 3129},
            output="bare-function-predicate",
        ),
    )
)
