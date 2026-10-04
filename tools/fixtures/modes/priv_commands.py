"""Registered fixture for exact privilege-command and obscene-name lookup."""

from __future__ import annotations

from pathlib import Path

from .gm_command_log import generate as generate_gm
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


GM_ACCOUNT = "PrivLookupGm"
GM_PASSWORD = "priv-gm-pw"
GM_CHARACTER = "PrivLookupGmCharacter"
PLAYER_ACCOUNT = "PrivLookupPlayer"
PLAYER_PASSWORD = "priv-player-pw"
PLAYER_CHARACTER = "PrivLookupPlayerCharacter"
CREATE_ACCOUNT = "PrivLookupCreate"
CREATE_PASSWORD = "priv-create-pw"
PLAYER_SERIAL = 2
OBSCENE_NAME = "BADNAME"
PLAYER_COMMAND = "PRIVPLAYER"
ADMIN_COMMAND = "PRIVADMIN"
UNLISTED_COMMAND = "PRIVUNLISTED"


def generate(output: Path) -> int:
    """Reuse the standard login fixture and add isolated privilege tables."""

    result = generate_gm(output)
    if result:
        return result

    accounts = output / "accounts" / "sphereaccu.scp"
    account_text = accounts.read_text(encoding="ascii")
    account_text = account_text.replace("GmCommandLogProbe", GM_ACCOUNT)
    account_text = account_text.replace("gm_cmd_log_pw", GM_PASSWORD)
    account_text = account_text.replace("GmCommandLogCharacter", GM_CHARACTER)
    account_text = account_text.replace("PLEVEL=Admin", "PLEVEL=GM")
    account_text = account_text.replace("GmCommandLogPlayer", PLAYER_ACCOUNT)
    account_text = account_text.replace("gm_cmd_player_pw", PLAYER_PASSWORD)
    account_text = account_text.replace(
        f"PASSWORD={PLAYER_PASSWORD}\nPLEVEL=Player\n[EOF]",
        f"PASSWORD={PLAYER_PASSWORD}\nPLEVEL=Player\nCHARUID={PLAYER_SERIAL}\nLASTCHARUID={PLAYER_SERIAL}\n"
        f"[{CREATE_ACCOUNT}]\nPASSWORD={CREATE_PASSWORD}\n[EOF]",
    )
    accounts.write_text(account_text, encoding="ascii")

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    char_text = char_text.replace("GmCommandLogProbe", GM_ACCOUNT).replace(
        "GmCommandLogCharacter", GM_CHARACTER
    )
    player = (
        "[WORLDCHAR c_MAN]\n"
        f"SERIAL={PLAYER_SERIAL}\n"
        f"ACCOUNT={PLAYER_ACCOUNT}\n"
        f"NAME={PLAYER_CHARACTER}\n"
        "EVENTS=e_AllPlayers\n"
        "STR=100\nDEX=100\nINT=100\nHITS=100\nMAXHITS=100\n"
        "MANA=100\nSTAM=100\nP=128,128,0\n"
    )
    if char_text.count("[EOF]") != 1:
        raise ValueError("privilege fixture save has an unexpected EOF count")
    chars.write_text(char_text.replace("[EOF]", player + "[EOF]", 1), encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[PLEVEL 1]
{PLAYER_COMMAND}

[PLEVEL 6]
{ADMIN_COMMAND}

[OBSCENE]
{OBSCENE_NAME}

[FUNCTION {PLAYER_COMMAND}]
SYSMESSAGE SPHERE_PRIV_PLAYER_OK
RETURN 0

[FUNCTION {ADMIN_COMMAND}]
SYSMESSAGE SPHERE_PRIV_ADMIN_OK
RETURN 0

[FUNCTION {UNLISTED_COMMAND}]
SYSMESSAGE SPHERE_PRIV_UNLISTED_OK
RETURN 0
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="priv-commands",
        fixture_args=None,
        order=96,
        id_block=96,
        case=FixtureCase(
            name="priv-commands",
            mode=None,
            tests=(TestCase("test_priv_commands.py", (), True, True),),
            ports={"native": 2960, "asan": 2961},
            generator="make_priv_commands_fixture.py",
            output="priv-commands",
        ),
    )
)
