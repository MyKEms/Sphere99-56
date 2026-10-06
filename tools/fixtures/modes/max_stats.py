"""Registered synthetic fixture for the stock stat/max-stat assignment rules."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "MaxStatsProbe"
PASSWORD = "max-pw"
EVENT_NAME = "e_MaxStatsProbe"
MARKER = "SPHERE_MAX_STATS"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    """Create a fresh character whose login script applies the stone values."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
STR=50
DEX=60
INTEL=110
VIT=80
SYSMESSAGE {MARKER} [<STR>|<DEX>|<INTEL>|<VIT>|<MAXHITS>|<MAXMANA>|<MAXSTAM>]
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
        'TITLE="Sphere synthetic max-stat fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic max-stat fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=MaxStatsProbeCharacter
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
        name="max-stats",
        fixture_args=(),
        order=167,
        id_block=112,
        case=FixtureCase(
            name="max-stats",
            mode="max-stats",
            tests=(TestCase("test_max_stats.py", (), True, True),),
            ports={"native": 3152, "asan": 3153},
            mode_by_variant={},
            output="max-stats",
        ),
    )
)
