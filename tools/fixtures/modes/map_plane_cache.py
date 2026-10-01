"""Registered synthetic fixture mode: map blocks shared by logical planes.

One world sector holds a player, NPCs and items on map plane 0 and on
several logical planes that read map 0 geometry: planes from 8 up fall back
to the plane 0 map entry, and planes 3-7 are separate entries that read the
map 0 files.  A driver item touches every 8x8 map block of the sector on
every plane each second, moves each NPC one step and back, and reports the
positions to the player's client.

The player uses a plane from 8 up: the plane 3-7 map entries require a
client resource level that the probe client does not announce.
"""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .compat_writer import UID_F_ITEM, write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "MapPlaneProbe"
PASSWORD = "plane-pw"
MARKER = "SPHERE_MAP_PLANE"
PLAYER_SERIAL = 3
PLAYER_PLANE = 10
PLAYER_POINT = (160, 160, 0)
# NPC serial, plane and position.  Every object keeps its own x,y so that
# objects on other planes never stand on a tile another object steps onto.
NPCS = (
    (4, 0, (150, 170, 0)),
    (5, 5, (154, 174, 0)),
    (6, 11, (158, 178, 0)),
)
SECTOR_ORIGIN = (128, 128)
SECTOR_BLOCKS = 8
BLOCK_SIZE = 8
BLOCK_OFFSET = 3
ITEM_PLANES = (0, 5, 10, 11, 35)
MARKER_ITEM_ID = 0x0EAA
DRIVER_ITEM_ID = 0x0EAB
MARKER_ITEM = "SYNTHETIC_MAP_PLANE_MARKER"
DRIVER_ITEM = "SYNTHETIC_MAP_PLANE_DRIVER"
DRIVER_SERIAL = UID_F_ITEM | 0x100
DRIVER_POINT = (186, 186, 0)
MARKER_SERIAL_BASE = UID_F_ITEM | 0x200


def marker_items() -> tuple[tuple[int, int, tuple[int, int, int]], ...]:
    """Return (serial, plane, point) for one item in every block and plane."""

    rows = []
    serial = MARKER_SERIAL_BASE
    for plane in ITEM_PLANES:
        for block_y in range(SECTOR_BLOCKS):
            for block_x in range(SECTOR_BLOCKS):
                point = (
                    SECTOR_ORIGIN[0] + block_x * BLOCK_SIZE + BLOCK_OFFSET,
                    SECTOR_ORIGIN[1] + block_y * BLOCK_SIZE + BLOCK_OFFSET,
                    0,
                )
                rows.append((serial, plane, point))
                serial += 1
    return tuple(rows)


def _point(point: tuple[int, int, int], plane: int) -> str:
    if plane:
        return f"{point[0]},{point[1]},{point[2]},{plane}"
    return f"{point[0]},{point[1]},{point[2]}"


def _scripts() -> str:
    fix_lines = "".join(
        f"FINDUID(0{serial:x}).FIX\n" for serial, _, _ in marker_items()
    )
    npc_lines = "".join(
        f"F_MAP_PLANE_NPC_STEP {plane},0{serial:x}\n" for serial, plane, _ in NPCS
    )
    player = f"0{PLAYER_SERIAL:x}"
    return f"""

[ITEMDEF 0x{MARKER_ITEM_ID:04X}]
DEFNAME={MARKER_ITEM}
NAME=synthetic map plane marker
TYPE=T_NORMAL

[ITEMDEF 0x{DRIVER_ITEM_ID:04X}]
DEFNAME={DRIVER_ITEM}
NAME=synthetic map plane driver
TYPE=T_NORMAL
ON=@Timer
TIMER=1
F_MAP_PLANE_DRIVER
RETURN 1

[FUNCTION F_MAP_PLANE_DRIVER]
; Read the terrain under one item in every block of the sector, per plane.
; Rows go only to the probe player, never to connections still logging in.
{fix_lines}{npc_lines}FINDUID({player}).SYSMESSAGE {MARKER} player|<FINDUID({player}).P>
FINDUID({player}).SYSMESSAGE {MARKER}_TICK
RETURN 1

[FUNCTION F_MAP_PLANE_NPC_STEP]
ARG(before,<FINDUID(<ARGV(1)>).P>)
FINDUID(<ARGV(1)>).WALK E
ARG(after,<FINDUID(<ARGV(1)>).P>)
FINDUID(<ARGV(1)>).WALK W
FINDUID({player}).SYSMESSAGE {MARKER} npc|<ARGV(0)>|<ARG.before>|<ARG.after>|<FINDUID(<ARGV(1)>).P>
RETURN 1

[AREA Synthetic shared plane sector]
P={_point((176, 176, 0), 255)}
RECT={SECTOR_ORIGIN[0]},{SECTOR_ORIGIN[1]},{SECTOR_ORIGIN[0] + 64},{SECTOR_ORIGIN[1] + 64}
"""


def generate(output: Path) -> int:
    """Generate one busy sector with objects on map-0 geometry planes."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    # MAPCACHETIME=0 makes every periodic sector pass drop all cached map
    # blocks, so the probe also covers freeing and reloading blocks while
    # the sector stays busy.
    ini_path = output / "sphere.ini"
    ini = ini_path.read_text(encoding="ascii")
    if ini.count("DEBUGLEVEL=0\n") != 1:
        raise RuntimeError("map plane fixture did not contain its debug level setting")
    ini_path.write_text(
        ini.replace("DEBUGLEVEL=0\n", "DEBUGLEVEL=0\nMAPCACHETIME=0\n"),
        encoding="ascii",
    )

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # The common login hook prints unrelated markers and creates items.
    text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    tables.write_text(text + _scripts(), encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID={PLAYER_SERIAL}
CHARUID={PLAYER_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    # ATTR_INVIS keeps the per-pass item updates off the player's client.
    items = "".join(
        f"""[WORLDITEM {MARKER_ITEM}]
SERIAL=0{serial:x}
ATTR=080
P={_point(point, plane)}
"""
        for serial, plane, point in marker_items()
    )
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic shared map plane fixture
VERSION=0.99
SAVECOUNT=0
{items}[WORLDITEM {DRIVER_ITEM}]
SERIAL=0{DRIVER_SERIAL:x}
P={_point(DRIVER_POINT, 0)}
TIMER=1
[EOF]
""",
        encoding="ascii",
    )
    npcs = "".join(
        f"""[WORLDCHAR c_MAN]
SERIAL=0{serial:x}
NAME=MapPlaneNpc{plane}
NPC=2
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P={_point(point, plane)}
"""
        for serial, plane, point in NPCS
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic shared map plane fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=0{PLAYER_SERIAL:x}
ACCOUNT={ACCOUNT}
NAME=MapPlaneProbe
DIR=0
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P={_point(PLAYER_POINT, PLAYER_PLANE)}
{npcs}[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=max(MARKER_ITEM_ID, DRIVER_ITEM_ID))
    return 0


MODE = register_mode(
    FixtureMode(
        name="map-plane-cache",
        fixture_args=(),
        order=81,
        id_block=81,
        case=FixtureCase(
            name="map-plane-cache",
            mode="map-plane-cache",
            tests=(TestCase("test_map_plane_cache.py", (), True, True),),
            ports={"native": 2888, "asan": 2889},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="map-plane-cache",
            test_args_by_variant={},
        ),
    )
)
