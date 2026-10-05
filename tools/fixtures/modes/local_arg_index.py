"""Registered synthetic fixture for ARG locals in EVAL and indexed DEFNAME loops."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "LocalArgProbe"
LOGIN_TOKEN = "local-arg-pw"
EVENT_NAME = "e_LocalArgProbe"
MARKER = "SPHERE_ARG_LOCAL_INDEX"

# (label, expected value).  The values are the reference server's: a bare ARG
# local inside EVAL reads the local, and an indexed DEFNAME past its last
# defined index is empty, so a WHILE over the indices stops after one pass.
ROWS = (
    ("evali", "2"),
    ("argi", "2"),
    ("bare", "2"),
    ("plain0", "0,20"),
    ("plain1", ""),
    ("idx0", "0,20"),
    ("idx1", ""),
    ("idxi", ""),
    ("loop_n", "1"),
    ("loop_j", "1"),
)


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES local_arg_index_defs]
def_ArgLocalIndexList[0] 0,20

[FUNCTION f_ArgLocalIndexProbe]
ARG(i,0)
ARG(i,#+1)
ARG(i,#+1)
SYSMESSAGE {MARKER} evali=[<EVAL i>]
SYSMESSAGE {MARKER} argi=[<ARG(i)>]
SYSMESSAGE {MARKER} bare=[<i>]
SYSMESSAGE {MARKER} plain0=[<def_ArgLocalIndexList[0]>]
SYSMESSAGE {MARKER} plain1=[<def_ArgLocalIndexList[1]>]
SYSMESSAGE {MARKER} idx0=[<SAFE.def_ArgLocalIndexList[0]>]
SYSMESSAGE {MARKER} idx1=[<SAFE.def_ArgLocalIndexList[1]>]
SYSMESSAGE {MARKER} idxi=[<SAFE.def_ArgLocalIndexList[<EVAL i>]>]
ARG(j,0)
ARG(r,<SAFE.def_ArgLocalIndexList[<EVAL j>]>)
ARG(n,0)
WHILE (STRLEN(<ARG(r)>)) && (<ARG(n)> < 5)
 ARG(j,#+1)
 ARG(n,#+1)
 ARG(r,<SAFE.def_ArgLocalIndexList[<EVAL j>]>)
ENDWHILE
SYSMESSAGE {MARKER} loop_n=[<ARG(n)>]
SYSMESSAGE {MARKER} loop_j=[<ARG(j)>]
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
f_ArgLocalIndexProbe
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
        """TITLE=Sphere synthetic ARG-local index fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic ARG-local index fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=LocalArgProbe
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
        name="local-arg-index",
        fixture_args=(),
        order=165,
        id_block=110,
        case=FixtureCase(
            name="local-arg-index",
            mode="local-arg-index",
            tests=(TestCase("test_local_arg_index.py", (), True, True),),
            ports={"native": 2976, "asan": 2977},
            output="local-arg-index",
        ),
    )
)
