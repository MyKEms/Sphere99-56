"""Registered synthetic fixture mode for stock-compatible TRY dispatch."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import GM_COMMAND_LOG_PLAYER_ACCOUNT, GM_COMMAND_LOG_PLAYER_PASSWORD
from .gm_command_log import generate as generate_gm
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


SERIAL = 2


def generate(output: Path) -> int:
    result = generate_gm(output)
    if result:
        return result

    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    if "UNKNOWNKEYWORDREPORT=" not in ini_text:
        ini_text = ini_text.replace(
            "LOGMASK=0x1ffff\n",
            "LOGMASK=0x1ffff\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        )
    ini.write_text(ini_text, encoding="ascii")

    scripts = output / "scripts" / "spheretables.scp"
    text = scripts.read_text(encoding="ascii")
    old = "    TRY S(NAME=<ARGS>)\n"
    if old not in text:
        raise ValueError("GM fixture did not contain the TRY dispatch source")
    # Keep the verb/argument shape used by accMsg while leaving the timestamp
    # escape to the production script.  The player invocation is the failing
    # case: stock records NO PRIV without reporting method TRY as unknown.
    text = text.replace(old, "    TRY S(<ARGS>)\n", 1)
    char_exists = (
        "[FUNCTION charExists]\n"
        "IF (safe finduid(<ARGV(0)>).isChar)\n"
        "  RETURN 1\n"
        "ENDIF\n"
        "RETURN 0\n"
    )
    if char_exists not in text:
        raise ValueError("GM fixture did not contain its charExists helper")
    # Force the stock accMsg fallback branch.  It dispatches SRC.TRY, which is
    # the path that the production unknown-keyword report exposed.
    text = text.replace(char_exists, "[FUNCTION charExists]\nRETURN 0\n", 1)
    item_probe = (
        "[ITEMDEF 0x0EAF]\n"
        "DEFNAME=SYNTHETIC_TRY_NOCLIENT\n"
        "NAME=synthetic TRY no-client item\n"
        "ON=@Create\n"
        "ON=@Timer\n"
        "TRY S(NOCONNECTION)\n"
    )
    if "DEFNAME=SYNTHETIC_TRY_NOCLIENT" not in text:
        text += "\n" + item_probe
    scripts.write_text(text, encoding="ascii")

    world = output / "save" / "sphereworld.scp"
    world_text = world.read_text(encoding="ascii")
    world_text = world_text.replace(
        "[EOF]\n",
        "[WORLDITEM SYNTHETIC_TRY_NOCLIENT]\nSERIAL=3\nATTR=DECAY\nTIMER=1\nP=128,128,0\n[EOF]\n",
        1,
    )
    world.write_text(world_text, encoding="ascii")

    accounts = output / "accounts" / "sphereaccu.scp"
    account_text = accounts.read_text(encoding="ascii")
    needle = f"[{GM_COMMAND_LOG_PLAYER_ACCOUNT}]\nPASSWORD={GM_COMMAND_LOG_PLAYER_PASSWORD}\n"
    replacement = needle + f"CHARUID={SERIAL}\nLASTCHARUID={SERIAL}\n"
    if needle not in account_text:
        raise ValueError("GM fixture player account was not found")
    accounts.write_text(account_text.replace(needle, replacement, 1), encoding="ascii")

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    marker = "[EOF]"
    player = (
        "[WORLDCHAR c_MAN]\n"
        f"SERIAL={SERIAL}\n"
        f"ACCOUNT={GM_COMMAND_LOG_PLAYER_ACCOUNT}\n"
        "NAME=GmCommandLogPlayer\n"
        "EVENTS=e_AllPlayers\n"
        "STR=100\nDEX=100\nINT=100\nHITS=100\nMAXHITS=100\n"
        "MANA=100\nSTAM=100\nP=128,128,0\n"
    )
    if char_text.count(marker) != 1:
        raise ValueError("GM fixture save has an unexpected EOF count")
    chars.write_text(char_text.replace(marker, player + marker, 1), encoding="ascii")
    return 0


MODE = register_mode(
    FixtureMode(
        name="try-verb",
        fixture_args=(),
        order=87,
        id_block=87,
        case=FixtureCase(
            name="try-verb",
            mode="try-verb",
            tests=(TestCase("test_try_verb.py", (), True, True),),
            ports={"native": 2896, "asan": 2897},
            output="try-verb",
        ),
    )
)
