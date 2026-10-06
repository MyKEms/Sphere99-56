"""Synthetic fixture for fixed-point skill text and numeric skill updates."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ExplevelValuesProbe"
PASSWORD = "explevel-pw"
EVENT_NAME = "e_ExplevelValuesProbe"
MARKER = "SPHERE_EXPLEVEL_VALUES"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    """Generate a self-contained character that exercises the level page values."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
TAG(hitspeed,20971)
TAG(SM,301535)
TAG(OM,362395)
SYSMESSAGE {MARKER} C|before|[<MAGERY>|<RESIST>|<eval MAGERY>|<eval RESIST>]
MAGERY=<eval MAGERY+10>
SYSMESSAGE {MARKER} C|after|[<MAGERY>|<RESIST>|<eval MAGERY>|<eval RESIST>]
SYSMESSAGE {MARKER} C|tags|[<tag.hitspeed>|<tag.SM>|<tag.OM>]
SYSMESSAGE {END_MARKER}
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
        'TITLE="Sphere synthetic explevel-values fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic explevel-values fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ExplevelValuesProbeCharacter
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
MAGERY=300
RESIST=0
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="explevel-values",
        fixture_args=(),
        order=168,
        id_block=113,
        case=FixtureCase(
            name="explevel-values",
            mode="explevel-values",
            tests=(TestCase("test_explevel_values.py", (), True, True),),
            ports={"native": 3154, "asan": 3155},
            mode_by_variant={},
            output="explevel-values",
        ),
    )
)
