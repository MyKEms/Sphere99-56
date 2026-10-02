"""Registered synthetic fixture for the self GM-mode command."""

from pathlib import Path
import struct

from .compat_writer import write_movement_tile, write_mul_fixture
from .gm_command_log import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode

ACCOUNT = "GmToggleProbe"
PASSWORD = "gm-pw"
CHAR_NAME = "GmToggleCharacter"
DAMAGE_ITEM_ID = 0x0EB2
DAMAGE_ITEM_SERIAL = 2
DAMAGE_ITEM_UID = 0x40000000 | DAMAGE_ITEM_SERIAL
BLOCKING_ITEM_ID = 0x0EB3
BLOCKING_POINT = (127, 127, 0)


def generate(output: Path) -> int:
    """Reuse the validated login world, with a dedicated GM identity."""
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    account_file = output / "accounts" / "sphereaccu.scp"
    account_text = account_file.read_text(encoding="ascii")
    account_text = (
        account_text.replace("GmCommandLogProbe", ACCOUNT)
        .replace("gm_cmd_log_pw", PASSWORD)
    )
    account_file.write_text(account_text, encoding="ascii")

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    char_text = char_text.replace("GmCommandLogProbe", ACCOUNT).replace(
        "GmCommandLogCharacter", CHAR_NAME
    )
    chars.write_text(char_text, encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{DAMAGE_ITEM_ID:04X}]
DEFNAME=GM_TOGGLE_DAMAGE
NAME=synthetic GM damage probe
TYPE=T_NORMAL
CAN=0x100
ON=@UserDClick
SRC.HITS=100
SRC.DAMAGE 10,0,<SRC.SERIAL>
SRC.SYSMESSAGE GM_TOGGLE_HITS <SRC.HITS>
RETURN 1

[ITEMDEF 0x{BLOCKING_ITEM_ID:04X}]
DEFNAME=GM_TOGGLE_BLOCKER
NAME=synthetic GM blocking probe
TYPE=T_NORMAL
CAN=0x8
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic GM toggle fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM GM_TOGGLE_DAMAGE]
SERIAL=0{DAMAGE_ITEM_UID:x}
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=BLOCKING_ITEM_ID)
    write_movement_tile(
        output / "muls" / "tiledata.mul", BLOCKING_ITEM_ID, 0x40, 15
    )
    # Put the immobile blocker in statics0.mul.  Dynamic items are searchable
    # objects, while this row exercises the same map collision path as a stock
    # wall or heavy object.
    block_x, block_y, block_z = BLOCKING_POINT
    block_index = (block_x // 8) * (0x1000 // 8) + (block_y // 8)
    with (output / "muls" / "staidx0.mul").open("r+b") as stream:
        stream.seek(block_index * 12)
        stream.write(struct.pack("<III", 0, 7, 0))
    (output / "muls" / "statics0.mul").write_bytes(
        struct.pack(
            "<HBBbH",
            BLOCKING_ITEM_ID,
            block_x % 8,
            block_y % 8,
            block_z,
            0,
        )
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="gm-toggle",
        fixture_args=None,
        # Blocks 87, 92, and 93 are assigned to other fixture modes.
        order=94,
        id_block=94,
        case=FixtureCase(
            name="gm-toggle",
            mode=None,
            tests=(TestCase("test_gm_toggle.py", (), True, True),),
            ports={"native": 2940, "asan": 2941},
            generator="make_gm_toggle_fixture.py",
            output="gm-toggle",
        ),
    )
)
