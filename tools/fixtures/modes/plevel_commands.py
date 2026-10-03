"""Registered synthetic fixture mode for [PLEVEL n] command privilege lists."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import GM_COMMAND_LOG_PLAYER_ACCOUNT, GM_COMMAND_LOG_PLAYER_PASSWORD
from .gm_command_log import generate as generate_gm
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


SERIAL = 2
MARKER = "PLEVEL_COMMAND_PROBE"
PLAYER_COMMAND = "plevelprobeplayer"
OWNER_COMMAND = "plevelprobeowner"
UNLISTED_COMMAND = "plevelprobeunlisted"


def generate(output: Path) -> int:
    result = generate_gm(output)
    if result:
        return result

    # The section names use both spellings found in shipped scripts.  A
    # command listed for players must be usable by a player; a command
    # listed only for owners must be refused to an admin; an unlisted
    # command keeps the GM default.  The privilege lookup reads only the
    # leading alphanumeric run of the verb, so the names avoid underscores.
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[plevel 1]
{PLAYER_COMMAND}

[PLEVEL 7]
{OWNER_COMMAND}

[FUNCTION {PLAYER_COMMAND}]
SYSMESSAGE {MARKER} player command ran
RETURN 1

[FUNCTION {OWNER_COMMAND}]
SYSMESSAGE {MARKER} owner command ran
RETURN 1

[FUNCTION {UNLISTED_COMMAND}]
SYSMESSAGE {MARKER} unlisted command ran
RETURN 1
""",
        encoding="ascii",
    )

    accounts = output / "accounts" / "sphereaccu.scp"
    account_text = accounts.read_text(encoding="ascii")
    needle = f"[{GM_COMMAND_LOG_PLAYER_ACCOUNT}]\nPASSWORD={GM_COMMAND_LOG_PLAYER_PASSWORD}\n"
    if needle not in account_text:
        raise ValueError("GM fixture player account was not found")
    accounts.write_text(
        account_text.replace(needle, needle + f"CHARUID={SERIAL}\nLASTCHARUID={SERIAL}\n", 1),
        encoding="ascii",
    )

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    marker = "[EOF]"
    if char_text.count(marker) != 1:
        raise ValueError("GM fixture save has an unexpected EOF count")
    player = (
        "[WORLDCHAR c_MAN]\n"
        f"SERIAL={SERIAL}\n"
        f"ACCOUNT={GM_COMMAND_LOG_PLAYER_ACCOUNT}\n"
        "NAME=PlevelCommandPlayer\n"
        "STR=100\nDEX=100\nINT=100\nHITS=100\nMAXHITS=100\n"
        "MANA=100\nSTAM=100\nP=129,128,0\n"
    )
    chars.write_text(char_text.replace(marker, player + marker, 1), encoding="ascii")
    return 0


MODE = register_mode(
    FixtureMode(
        name="plevel-commands",
        fixture_args=(),
        order=152,
        id_block=97,
        case=FixtureCase(
            name="plevel-commands",
            mode="plevel-commands",
            tests=(TestCase("test_plevel_commands.py", (), True, True),),
            ports={"native": 2994, "asan": 2995},
            output="plevel-commands",
        ),
    )
)
