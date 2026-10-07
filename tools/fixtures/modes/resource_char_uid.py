"""Synthetic fixture for 0.99 resource-UID character save sections."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ResourceUidCharProbe"
PASSWORD = "uid-pw"
EVENT_NAME = "e_ResourceUidCharProbe"
MARKER = "SPHERE_RESOURCE_UID_CHAR"
CHARDEF_INDEX = 0x0122


def generate(output: Path) -> int:
    """Generate a minimal world whose character type is a full resource UID."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[CHARDEF 0x{CHARDEF_INDEX:04X}]
DEFNAME=SYNTHETIC_RESOURCE_UID_CHAR
NAME=resource UID fixture character
STR=177
INT=177
DEX=177
ARMOR=77

[EVENTS {EVENT_NAME}]
ON=@Login
SYSMESSAGE {MARKER} <SRC.NAME>|<SRC.STR>
SYSMESSAGE {MARKER}_END
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic resource-UID character fixture
VERSION=0.99z8
SAVECOUNT=0
[WORLDCHAR #08e000122]
SERIAL=3
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
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic resource-UID character fixture\n"
        "VERSION=0.99z8\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="resource-char-uid",
        fixture_args=None,
        order=173,
        id_block=120,
        case=FixtureCase(
            name="resource-char-uid",
            mode=None,
            tests=(TestCase("test_resource_char_uid.py", (), True, True),),
            ports={"native": 3002, "asan": 3003},
            generator="make_resource_char_uid_fixture.py",
            output="resource-char-uid",
        ),
    )
)
