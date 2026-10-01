"""Registered synthetic fixture mode for an official-client game login."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output: Path) -> int:
    """Build a small runtime with the account named by the captured login."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    (output / "accounts" / "sphereaccu.scp").write_text(
        "[test_player]\n"
        "PASSWORD=test-pass\n"
        "CHARUID=1\n"
        "LASTCHARUID=1\n"
        "[EOF]\n",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    save_header = "TITLE=Sphere official crypt fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(
        save_header + "[EOF]\n", encoding="ascii"
    )
    (output / "save" / "spherechars.scp").write_text(
        save_header
        + "[WORLDCHAR c_MAN]\n"
        + "SERIAL=1\n"
        + "ACCOUNT=test_player\n"
        + "NAME=Test Player\n"
        + "STR=100\nINT=100\nDEX=100\nHITS=100\nMAXHITS=100\n"
        + "MANA=100\nSTAM=100\nP=128,128,0\n"
        + "[EOF]\n",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="official-crypt",
        fixture_args=(),
        order=86,
        id_block=86,
        case=FixtureCase(
            name="official-crypt",
            mode="official-crypt",
            tests=(TestCase("test_official_crypt.py", (), True, True),),
            ports={"native": 2941, "asan": 2942},
            output="official-crypt",
        ),
    )
)
