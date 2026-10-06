"""Registered fixture for player-visible character diagnostic tags."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import write_mul_fixture
from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


PLAYER_ACCOUNT = "InvulTagsPlayer"
PLAYER_PASSWORD = "invul-player-pw"
PLAYER_SERIAL = 3
STAFF_ACCOUNT = "InvulTagsStaff"
STAFF_PASSWORD = "invul-staff-pw"
STAFF_SERIAL = 4
NPC_SERIAL = 5
NPC_DEFNAME = "SYNTHETIC_INVUL_TAG_NPC"
NPC_NAME = "InvulTagProbe"


def _account_file() -> str:
    return f"""[ACCOUNT {PLAYER_ACCOUNT}]
PASSWORD={PLAYER_PASSWORD}
PLEVEL=Player
CHARUID={PLAYER_SERIAL}
LASTCHARUID={PLAYER_SERIAL}
[ACCOUNT {STAFF_ACCOUNT}]
PASSWORD={STAFF_PASSWORD}
PLEVEL=GM
PRIV=0x4000
CHARUID={STAFF_SERIAL}
LASTCHARUID={STAFF_SERIAL}
[EOF]
"""


def _character(
    serial: int,
    account: str,
    name: str,
    point: str,
    *,
    npc: bool = False,
) -> str:
    npc_line = "NPC=2\n" if npc else ""
    flags = "Flag_Invul=1\nFlag_Stone=1\nFlag_Immobile=1\n" if npc else ""
    return f"""[WORLDCHAR {NPC_DEFNAME if npc else 'c_MAN'}]
SERIAL={serial}
ACCOUNT={account}
NAME={name}
{npc_line}STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
{flags}P={point}
"""


def generate(output: Path) -> int:
    """Generate two viewers and one invulnerable world NPC."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini_path = output / "sphere.ini"
    ini = ini_path.read_text(encoding="ascii")
    if ini.count("DEBUGLEVEL=0") != 1 or "CHARTAGS=0" in ini:
        raise RuntimeError("unexpected base fixture configuration")
    ini_path.write_text(ini.replace("DEBUGLEVEL=0", "DEBUGLEVEL=0\nCHARTAGS=0"), encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[CHARDEF {NPC_DEFNAME}]
DEFNAME={NPC_DEFNAME}
NAME={NPC_NAME}
ID=0x0190
NPC=brain_human
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        _account_file(), encoding="ascii"
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic character-tag fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    chars = """TITLE=Sphere synthetic character-tag fixture
VERSION=0.99
SAVECOUNT=0
"""
    chars += _character(PLAYER_SERIAL, PLAYER_ACCOUNT, "InvulPlayer", "128,128,0")
    chars += _character(STAFF_SERIAL, STAFF_ACCOUNT, "InvulStaff", "128,129,0")
    chars += _character(NPC_SERIAL, "", NPC_NAME, "130,128,0", npc=True)
    # A world NPC is not account-owned.  Keep its section valid without an
    # ACCOUNT key so the probe exercises the broadcast NPC label path.
    chars = chars.replace("ACCOUNT=\n", "")
    chars += "[EOF]\n"
    (output / "save" / "spherechars.scp").write_text(chars, encoding="ascii")
    write_mul_fixture(output)
    return 0


MODE = register_mode(
    FixtureMode(
        name="invul-tags",
        fixture_args=(),
        order=169,
        id_block=114,
        case=FixtureCase(
            name="invul-tags",
            mode="invul-tags",
            tests=(TestCase("test_invul_tags.py", (), True, True),),
            ports={"native": 3160, "asan": 3161},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="invul-tags",
            test_args_by_variant={},
        ),
    )
)
