"""Registered synthetic fixture mode for map-plane visibility.

The player and one NPC/item are on logical plane 10.  Matching objects are
also placed at the same coordinates on plane 0.  The fixture deliberately
keeps those objects in the same sectors so a distance-only search cannot
hide the regression by taking a different sector path.
"""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import UID_F_ITEM, write_movement_tile, write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "PlaneVisibilityProbe"
PASSWORD = "pv-pw"
PLAYER_SERIAL = 3
PLAYER_PLANE = 10
PLAYER_POINT = (128, 128, 0)

SAME_PLANE_NPC_SERIAL = 4
OTHER_PLANE_NPC_SERIAL = 5
SAME_PLANE_NPC_POINT = (130, 128, 0)
OTHER_PLANE_NPC_POINT = SAME_PLANE_NPC_POINT

SAME_PLANE_ITEM_SERIAL = 0x110
OTHER_PLANE_ITEM_SERIAL = 0x111
OTHER_PLANE_BLOCKER_SERIAL = 0x112
SAME_PLANE_DESTINATION_ITEM_SERIAL = 0x113
SAME_PLANE_ITEM_POINT = PLAYER_POINT
OTHER_PLANE_ITEM_POINT = PLAYER_POINT
DESTINATION_POINT = (129, 128, 0)

VISIBLE_ITEM_ID = 0x0EB0
BLOCKING_ITEM_ID = 0x0EB1
NPC_DEFNAME = "SYNTHETIC_PLANE_VIS_NPC"
VISIBLE_ITEM_DEFNAME = "SYNTHETIC_PLANE_VIS_ITEM"
BLOCKING_ITEM_DEFNAME = "SYNTHETIC_PLANE_VIS_BLOCKER"


def _point(point: tuple[int, int, int], plane: int) -> str:
    if plane:
        return f"{point[0]},{point[1]},{point[2]},{plane}"
    return f"{point[0]},{point[1]},{point[2]}"


def _uid(serial: int) -> str:
    return f"0{UID_F_ITEM | serial:x}"


def _scripts() -> str:
    return f"""

[ITEMDEF 0x{VISIBLE_ITEM_ID:04X}]
DEFNAME={VISIBLE_ITEM_DEFNAME}
NAME=synthetic plane visibility item
TYPE=T_NORMAL
CAN=0x100

[ITEMDEF 0x{BLOCKING_ITEM_ID:04X}]
DEFNAME={BLOCKING_ITEM_DEFNAME}
NAME=synthetic plane visibility blocker
TYPE=T_NORMAL
CAN=0x8

[CHARDEF {NPC_DEFNAME}]
DEFNAME={NPC_DEFNAME}
NAME=synthetic plane visibility NPC
ID=0x0190
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
"""


def generate(output: Path) -> int:
    """Generate same-plane and cross-plane objects in one synthetic sector."""

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
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic map-plane visibility fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM {VISIBLE_ITEM_DEFNAME}]
SERIAL={_uid(SAME_PLANE_ITEM_SERIAL)}
P={_point(SAME_PLANE_ITEM_POINT, PLAYER_PLANE)}
[WORLDITEM {VISIBLE_ITEM_DEFNAME}]
SERIAL={_uid(OTHER_PLANE_ITEM_SERIAL)}
P={_point(OTHER_PLANE_ITEM_POINT, 0)}
[WORLDITEM {BLOCKING_ITEM_DEFNAME}]
SERIAL={_uid(OTHER_PLANE_BLOCKER_SERIAL)}
P={_point(DESTINATION_POINT, 0)}
[WORLDITEM {VISIBLE_ITEM_DEFNAME}]
SERIAL={_uid(SAME_PLANE_DESTINATION_ITEM_SERIAL)}
P={_point(DESTINATION_POINT, PLAYER_PLANE)}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic map-plane visibility fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={PLAYER_SERIAL}
ACCOUNT={ACCOUNT}
NAME=PlaneVisibilityProbe
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P={_point(PLAYER_POINT, PLAYER_PLANE)}
[WORLDCHAR {NPC_DEFNAME}]
SERIAL={SAME_PLANE_NPC_SERIAL}
NAME=SamePlaneNpc
NPC=2
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P={_point(SAME_PLANE_NPC_POINT, PLAYER_PLANE)}
[WORLDCHAR {NPC_DEFNAME}]
SERIAL={OTHER_PLANE_NPC_SERIAL}
NAME=OtherPlaneNpc
NPC=2
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P={_point(OTHER_PLANE_NPC_POINT, 0)}
[EOF]
""",
        encoding="ascii",
    )

    write_mul_fixture(output, extra_item_id=BLOCKING_ITEM_ID)
    # The dynamic blocker must be physically blocking when its plane is
    # accidentally considered.  The fixed implementation filters it before
    # this tiledata record can affect height.
    write_movement_tile(output / "muls" / "tiledata.mul", BLOCKING_ITEM_ID, 0x40, 15)
    return 0


MODE = register_mode(
    FixtureMode(
        name="plane-visibility",
        fixture_args=(),
        order=83,
        id_block=83,
        case=FixtureCase(
            name="plane-visibility",
            mode="plane-visibility",
            tests=(TestCase("test_plane_visibility.py", (), True, True),),
            ports={"native": 2890, "asan": 2891},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="plane-visibility",
            test_args_by_variant={},
        ),
    )
)
