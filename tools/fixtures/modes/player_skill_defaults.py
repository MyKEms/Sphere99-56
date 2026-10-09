"""Registered fixture for fresh-player skill defaults."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT_BY_LIMIT = {
    0: "SkillZero",
    25: "SkillNxx",
}
PASSWORD = "psd-pw"
CHARACTER_BY_LIMIT = {
    0: "SkillZero",
    25: "SkillNxx",
}
MARKER = "SPHERE_PLAYER_SKILL_DEFAULTS"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini_path = output / "sphere.ini"
    ini = ini_path.read_text(encoding="ascii")
    ini_path.write_text(
        ini.replace("DEBUGLEVEL=0\n", "DEBUGLEVEL=0\nMAXBASESKILL=0\n", 1),
        encoding="ascii",
    )

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    marker = ""
    for character in CHARACTER_BY_LIMIT.values():
        marker += (
            f"IF (<STRMATCH <NAME>,{character}>)\n"
            f"SYSMESSAGE {MARKER} [<eval <findres(skill,16).name>>|<eval <findres(skill,26).name>>]\n"
            f"SYSMESSAGE {END_MARKER}\n"
            "ENDIF\n"
        )
    text = text.replace("[EVENTS e_AllPlayers]\nON=@LogIn\n", "[EVENTS e_AllPlayers]\nON=@LogIn\n" + marker, 1)
    tables.write_text(text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        "".join(
            f"[ACCOUNT {account}]\nPASSWORD={PASSWORD}\n"
            for account in ACCOUNT_BY_LIMIT.values()
        )
        + "[EOF]\n",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic fresh-player skill fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        'TITLE="Sphere synthetic fresh-player skill fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="player-skill-defaults",
        fixture_args=(),
        order=181,
        id_block=128,
        case=FixtureCase(
            name="player-skill-defaults",
            mode="player-skill-defaults",
            tests=(
                TestCase("test_player_skill_defaults.py", ("--max-base-skill", "0"), True, True),
                TestCase("test_player_skill_defaults.py", ("--max-base-skill", "25"), True, True),
            ),
            ports={"native": 4620, "asan": 4621},
            output="player-skill-defaults",
        ),
    )
)
