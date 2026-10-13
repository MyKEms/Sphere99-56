"""Synthetic fixture for dynamic EVAL of a returned object property name."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "EvalObjectPropertyProbe"
LOGIN_VALUE = "eval-pw"
EVENT_NAME = "e_EvalObjectPropertyProbe"
MARKER = "SPHERE_EVAL_OBJECT_PROPERTY"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[FUNCTION f_EvalObjectProperty]
ARG(dynamic,<eval argv(0)>)
ARG(arithmetic,<eval 2+3>)
ARG(implicit,<eval f_EvalImplicitReturn>)
SYSMESSAGE {MARKER} dynamic=[<arg(dynamic)>] arithmetic=[<arg(arithmetic)>] implicit=[<arg(implicit)>]
RETURN 1

[FUNCTION f_EvalInnerReturn]
RETURN 1

[FUNCTION f_EvalImplicitReturn]
f_EvalInnerReturn

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_EvalObjectProperty("Stamina")
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic dynamic EVAL fixture\n"
        "VERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic dynamic EVAL fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=EvalObjectPropertyProbe
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
    return 0


MODE = register_mode(
    FixtureMode(
        name="eval-object-property",
        fixture_args=(),
        order=199,
        id_block=150,
        case=FixtureCase(
            name="eval-object-property",
            mode="eval-object-property",
            tests=(TestCase("test_eval_object_property.py", (), True, True),),
            ports={"native": 43207, "asan": 43208},
            output="eval-object-property",
        ),
    )
)
