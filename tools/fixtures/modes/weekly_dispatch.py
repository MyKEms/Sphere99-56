"""Synthetic coverage for indexed DEFNAME function dispatch."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "WeeklyDispatchProbe"
PASSWORD = "weekly-pw"
EVENT_NAME = "e_WeeklyDispatchProbe"
MARKER = "SPHERE_WEEKLY_DISPATCH"
END_MARKER = MARKER + " end"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES weekly_dispatch_probe]
def_weekly_low_setupFunction[0] f_weekly_dispatch_target
def_weekly_low_setupFunArgn[0] 1
def_weekly_low_setupFunArg_0_[0] probe

[FUNCTION f_weekly_dispatch_target]
SYSMESSAGE {MARKER} target|[<argv(0)>]
RETURN 1

[FUNCTION f_weekly_dispatch]
ARG(arguments,"")
ARG(argNumb,<def_weekly_<argv(1)>_setupFunArgn[<eval argv(0)>]>)
ARG(i,0)
WHILE (<arg(i)> < <arg(argNumb)>)
  IF (<arg(i)>)
    ARG(arguments,"<arg(arguments)>,")
  ENDIF
  ARG(arguments,"<arg(arguments)><def_weekly_<argv(1)>_setupFunArg_<eval argv(0)>_[<eval arg(i)>]>")
  ARG(i,#+1)
ENDWHILE
SYSMESSAGE {MARKER} selected|[<def_weekly_<argv(1)>_setupFunction[<eval argv(0)>]>]
<def_weekly_<argv(1)>_setupFunction[<eval argv(0)>]>(<arg(arguments)>)
SYSMESSAGE {END_MARKER}
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_weekly_dispatch(0,low)
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic weekly dispatch fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic weekly dispatch fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME={ACCOUNT}
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
STR=36
INT=11
DEX=36
HITS=36
MAXHITS=36
MANA=11
MAXMANA=11
STAM=36
MAXSTAM=36
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="weekly-dispatch",
        fixture_args=(),
        order=182,
        id_block=129,
        case=FixtureCase(
            name="weekly-dispatch",
            mode="weekly-dispatch",
            tests=(TestCase("test_weekly_dispatch.py", (), True, True),),
            ports={"native": 3162, "asan": 3163},
            output="weekly-dispatch",
        ),
    )
)
