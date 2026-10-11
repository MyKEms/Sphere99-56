"""Registered fixture for NPC ticks on a high logical map plane."""

from pathlib import Path

from .compat_writer import (
    write_container_tile,
    write_mul_fixture,
    write_runtime_files,
    write_scripts,
)
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NpcPlaneWakeProbe"
PASSWORD = "npw"
PLAYER_SERIAL = 3
NPC_SERIAL = 4
DRIVER_SERIAL = 0x131
DRIVER_UID = 0x40000000 | DRIVER_SERIAL
DRIVER_ITEM_ID = 0x0E9A
PLAYER_UID = 0x40000000 | PLAYER_SERIAL
NPC_UID = 0x40000000 | NPC_SERIAL
PLANE = 35
POINT = (128, 128, 0)
MARKER = "SPHERE_NPC_PLANE_WAKE"


def _point(point: tuple[int, int, int], plane: int) -> str:
    return f"{point[0]},{point[1]},{point[2]},{plane}"


def generate(output: Path) -> int:
    """Create a client and hostile NPC in the same high-plane sector."""

    write_runtime_files(output)
    write_scripts(output)

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{DRIVER_ITEM_ID:04X}]
DEFNAME=SYNTHETIC_NPC_PLANE_WAKE_DRIVER
NAME=synthetic NPC plane wake driver
TYPE=T_NORMAL
ON=@Timer
TIMER=1
FINDUID({NPC_UID}).ATTACK {PLAYER_UID}
FINDUID({PLAYER_UID}).SYSMESSAGE {MARKER}|<FINDUID({PLAYER_UID}).HITS>
RETURN 1

[CHARDEF SYNTHETIC_NPC_PLANE_WAKE_NPC]
DEFNAME=SYNTHETIC_NPC_PLANE_WAKE_NPC
NAME=synthetic high-plane attacker
ID=0x0190
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
DAM=100,100
WRESTLING=100.0
NPC=2
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[{ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID={PLAYER_SERIAL}
LASTCHARUID={PLAYER_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic high-plane NPC fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM SYNTHETIC_NPC_PLANE_WAKE_DRIVER]
SERIAL={DRIVER_UID}
P={_point(POINT, PLANE)}
TIMER=1
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic high-plane NPC fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={PLAYER_SERIAL}
ACCOUNT={ACCOUNT}
NAME=NpcPlaneWakePlayer
STR=100
INT=100
DEX=100
HITS=1000
MAXHITS=1000
MANA=100
STAM=100
P={_point(POINT, PLANE)}
[WORLDCHAR SYNTHETIC_NPC_PLANE_WAKE_NPC]
SERIAL={NPC_SERIAL}
NAME=NpcPlaneWakeAttacker
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
NPC=2
P={_point((129, 128, 0), PLANE)}
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=DRIVER_ITEM_ID)
    # Login creates the normal backpack (0x2007); give the synthetic MUL set
    # the same container record as the other character fixtures.
    write_container_tile(output / "muls" / "tiledata.mul", 0x2007)
    return 0


MODE = register_mode(
    FixtureMode(
        name="npc-plane-wake",
        fixture_args=(),
        order=183,
        id_block=134,
        case=FixtureCase(
            name="npc-plane-wake",
            mode="npc-plane-wake",
            tests=(TestCase("test_npc_plane_wake.py", (), True, True),),
            ports={"native": 4640, "asan": 4641},
            output="npc-plane-wake",
        ),
    )
)
