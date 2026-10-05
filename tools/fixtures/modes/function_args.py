"""Registered synthetic fixture for script-function arguments and references."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "FunctionArgsProbe"
LOGIN_TOKEN = "function-args-pw"
EVENT_NAME = "e_FunctionArgsProbe"
MARKER = "SPHERE_FUNCTION_ARGS"

# (label, call form, argument text, expected ARGS, expected ARGVCOUNT).  The
# values are the reference server's: each comma-separated argument that is
# pure arithmetic arrives evaluated, everything else arrives as written.
ROWS = (
    ("angle_expr", "angle", "(100/10)-1", "9", "1"),
    ("angle_sum", "angle", "20+3", "23", "1"),
    ("angle_plain", "angle", "105", "105", "1"),
    ("statement_expr", "statement", "(100/10)-1", "9", "1"),
    ("angle_two", "angle", "1,2+3", "1,5", "2"),
    ("angle_spaced", "angle", "5 - 1", "4", "1"),
    ("angle_unary", "angle", "-3+5", "2", "1"),
    ("angle_negative", "angle", "-7", "-7", "1"),
    ("angle_hex_literal", "angle", "010", "010", "1"),
    ("angle_word", "angle", "abc", "abc", "1"),
    ("angle_mixed", "angle", "5-1abc", "5-1abc", "1"),
    ("angle_large", "angle", "(1054696270/10)-1", "105469626", "1"),
    ("angle_quoted", "angle", '"5-1"', "5-1", "1"),
)


def _probe_lines() -> str:
    lines = []
    for label, form, argument, _, _ in ROWS:
        lines.append(f"TAG.FNARGS_LABEL={label}")
        if form == "angle":
            lines.append(f"ARG(FNARGS_DISCARD,<f_FunctionArgsEcho({argument})>)")
        else:
            lines.append(f"f_FunctionArgsEcho({argument})")
    lines.append(f"SYSMESSAGE {MARKER} done")
    lines.append("SYSMESSAGE SPHERE_FUNCTION_ARGS_FINDRES_DIRECT [<findres(skill,0).name>]")
    lines.append("SYSMESSAGE SPHERE_FUNCTION_ARGS_FINDRES [<f_FunctionArgsFindRes(0)>]")
    return "\n".join(lines)


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[FUNCTION f_FunctionArgsEcho]
SYSMESSAGE {MARKER} <TAG.FNARGS_LABEL> args=[<ARGS>] count=[<ARGVCOUNT>]
RETURN 1

[FUNCTION f_FunctionArgsFindRes]
RETURN "<findres(skill,args).name>"

[SKILL 0]
KEY=SYNTH_SKILL_0
TITLE=synthetic skill

[EVENTS {EVENT_NAME}]
ON=@LogIn
{_probe_lines()}
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
        """TITLE=Sphere synthetic function-argument fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic function-argument fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=FunctionArgsProbe
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
        name="function-args",
        fixture_args=(),
        order=153,
        id_block=98,
        case=FixtureCase(
            name="function-args",
            mode="function-args",
            tests=(TestCase("test_function_args.py", (), True, True),),
            ports={"native": 2996, "asan": 2997},
            output="function-args",
        ),
    )
)
