"""Synthetic fixture for numeric EVAL of an empty script-function argument."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ExplevelEmptyArgv"
PASSWORD = "exea-pw"
EVENT_NAME = "e_ExplevelEmptyArgv"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[FUNCTION f_emptyArgv]
textA(20,20,3,"<?eval argv(1)?>")
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
DIALOG d_empty_argv
RETURN 0

[DIALOG d_empty_argv]
0 0
resizepic 0 0 5054 300 100
argo.f_emptyArgv("Mana regeneration","")

[DIALOG d_empty_argv TEXT]

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
        'TITLE="Sphere synthetic empty ARGV fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic empty ARGV fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ExplevelEmptyArgvCharacter
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
        name="explevel-empty-argv",
        fixture_args=(),
        order=180,
        id_block=127,
        case=FixtureCase(
            name="explevel-empty-argv",
            mode="explevel-empty-argv",
            tests=(TestCase("test_explevel_empty_argv.py", (), True, True),),
            ports={"native": 4630, "asan": 4631},
            mode_by_variant={},
            output="explevel-empty-argv",
        ),
    )
)
