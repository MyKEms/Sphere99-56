"""Registered fixture for nested region precedence and plane matching."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "RegionShadowingProbe"
PASSWORD = "rsh-pw"
MARKER = "SPHERE_REGION_SHADOW"
EVENT = "e_region_shadowing"
PLAYER_SERIAL = 3


def _scripts() -> str:
    return f"""

[AREA SYNTHETIC WORLD]
P=320,320,0
RECT=0,0,640,640
MAPPLANE=255
FLAGS=01000

[AREA SYNTHETIC INNER]
P=300,300,0
RECT=290,290,310,310
MAPPLANE=0
FLAGS=02000

[AREA SYNTHETIC OTHER PLANE]
P=300,300,0,100
RECT=290,290,310,310
MAPPLANE=100
FLAGS=04000

[AREA SYNTHETIC REVERSED]
P=400,400,0
RECT=410,410,390,390
MAPPLANE=0
FLAGS=08000

[EVENTS {EVENT}]
ON=@Login
P=300,300,0
SYSMESSAGE {MARKER} inside|[<REGION.NAME>]|[<REGION.FLAGS>]|[<P>]
P=300,300,0,100
SYSMESSAGE {MARKER} plane|[<REGION.NAME>]|[<REGION.FLAGS>]|[<P>]
P=400,400,0
SYSMESSAGE {MARKER} reversed|[<REGION.NAME>]|[<REGION.FLAGS>]|[<P>]
P=320,320,0
SYSMESSAGE {MARKER} outside|[<REGION.NAME>]|[<REGION.FLAGS>]|[<P>]
SYSMESSAGE {MARKER}_END
RETURN 0
"""


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii") + _scripts(),
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID={PLAYER_SERIAL}
CHARUID={PLAYER_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic region shadowing fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic region shadowing fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={PLAYER_SERIAL}
NAME=RegionShadowingProbeCharacter
ACCOUNT={ACCOUNT}
EVENTS={EVENT}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
P=320,320,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="region-shadowing",
        fixture_args=(),
        order=200,
        id_block=151,
        case=FixtureCase(
            name="region-shadowing",
            mode="region-shadowing",
            tests=(TestCase("test_region_shadowing.py", (), True, True),),
            ports={"native": 43209, "asan": 43210},
            mode_by_variant={},
            output="region-shadowing",
        ),
    )
)
