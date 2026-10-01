"""Registered synthetic fixture mode: client packet bursts and floods."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "PacketBurstProbe"
PASSWORD = "burst-pw"
CHAR_SERIAL = 3
# World items in view of the character, saved with the item UID flag.
ITEM_SERIALS = tuple(0x40000010 + index for index in range(10))
ITEM_POSITIONS = tuple((126 + index % 5, 125 + 2 * (index // 5), 0) for index in range(10))


def generate(output: Path) -> int:
    """Generate one existing character surrounded by clickable world items."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # The common login hook prints unrelated markers and creates items.  This
    # mode measures packet timing, so the login must stay quiet.
    text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    tables.write_text(text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID={CHAR_SERIAL}
CHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    items = "".join(
        f"""[WORLDITEM SYNTHETIC_OBJECT]
SERIAL=0{serial:x}
P={x},{y},{z}
"""
        for serial, (x, y, z) in zip(ITEM_SERIALS, ITEM_POSITIONS)
    )
    # Both files of a save carry the same SAVECOUNT header.
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic packet burst fixture
VERSION=0.99
SAVECOUNT=0
{items}[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic packet burst fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME=PacketBurstProbe
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
        name="packet-burst",
        fixture_args=(),
        order=78,
        id_block=78,
        case=FixtureCase(
            name="packet-burst",
            mode="packet-burst",
            tests=(TestCase("test_packet_burst.py", (), True, True),),
            ports={"native": 2884, "asan": 2885},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="packet-burst",
            test_args_by_variant={},
        ),
    )
)
