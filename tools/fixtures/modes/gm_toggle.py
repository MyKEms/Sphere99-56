"""Registered synthetic fixture for the self GM-mode command."""

from pathlib import Path

from .compat_writer import write_mul_fixture
from .gm_command_log import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode

ACCOUNT = "GmToggleProbe"
PASSWORD = "gm-pw"
CHAR_NAME = "GmToggleCharacter"
PLAYER_ACCOUNT = "GmTogglePlayer"
PLAYER_PASSWORD = "gm-player-pw"
PLAYER_SERIAL = 3
DAMAGE_ITEM_ID = 0x0EB2
DAMAGE_ITEM_SERIAL = 2
DAMAGE_ITEM_UID = 0x40000000 | DAMAGE_ITEM_SERIAL


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
        .replace("GmCommandLogPlayer", PLAYER_ACCOUNT)
        .replace("gm_cmd_player_pw", PLAYER_PASSWORD)
    )
    account_text = account_text.replace(
        f"[{PLAYER_ACCOUNT}]\nPASSWORD={PLAYER_PASSWORD}\nPLEVEL=Player",
        f"[{PLAYER_ACCOUNT}]\nPASSWORD={PLAYER_PASSWORD}\nPLEVEL=Player\n"
        f"CHARUID={PLAYER_SERIAL}\nLASTCHARUID={PLAYER_SERIAL}",
    )
    account_file.write_text(account_text, encoding="ascii")

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    char_text = char_text.replace("GmCommandLogProbe", ACCOUNT).replace(
        "GmCommandLogCharacter", CHAR_NAME
    )
    char_text = char_text.replace(
        "[EOF]\n",
        f"[WORLDCHAR c_MAN]\nSERIAL={PLAYER_SERIAL}\nACCOUNT={PLAYER_ACCOUNT}\n"
        "NAME=GmTogglePlayerCharacter\nEVENTS=e_AllPlayers\nSTR=100\nDEX=100\n"
        "INT=100\nHITS=100\nMAXHITS=100\nMANA=100\nSTAM=100\nP=140,128,0\n[EOF]\n",
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
    write_mul_fixture(output)
    return 0


MODE = register_mode(
    FixtureMode(
        name="gm-toggle",
        fixture_args=None,
        # Keep the GM fixture after every allocation in the current manifest.
        order=171,
        id_block=118,
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
