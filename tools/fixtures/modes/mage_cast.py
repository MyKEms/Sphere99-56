"""Registered fixture for the Magery success path and spell state."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import (
    write_equipment_tile,
    write_mul_fixture,
    write_runtime_files,
    write_scripts,
)
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "MageCastProbe"
PASSWORD = "mage-pw"
CHAR_SERIAL = 3
BOOK_SERIAL = 4
PACK_SERIAL = 5
EVENT_NAME = "e_MageCastProbe"
SPELLBOOK_ID = 0x0EFA
MARKER = "SPHERE_MAGE"
CAST_MARKER = MARKER + "_CAST"
SUCCESS_MARKER = MARKER + "_SKILL_SUCCESS"
FAIL_MARKER = MARKER + "_SKILL_FAIL"
READY_MARKER = MARKER + "_READY"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    """Create a Magery 30 player with a full spellbook."""

    write_runtime_files(output)
    write_scripts(output)

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # MAGERY is the first synthetic skill after the generated skill table's
    # zero-based placeholders.  Keep the handler attached to the actual
    # Magery section; the following skill is RESIST.
    skill_marker = "\n[SKILL 25]\n"
    next_skill_marker = "\n[SKILL 26]\n"
    skill_handlers = f"""
ON=@Success
SYSMESSAGE {SUCCESS_MARKER}|[<ACTARG1>]
RETURN 0
ON=@Fail
SYSMESSAGE {FAIL_MARKER}|[<ACTARG1>]
RETURN 0
"""
    if skill_marker not in text or next_skill_marker not in text:
        raise ValueError("synthetic Magery skill section was not generated")
    text = text.replace(next_skill_marker, skill_handlers + next_skill_marker, 1)
    text += f"""

[TYPEDEF 107]
DEFNAME=T_SPELLBOOK

[ITEMDEF 0x{SPELLBOOK_ID:04X}]
DEFNAME=SYNTHETIC_SPELLBOOK
NAME=synthetic spellbook
TYPE=T_SPELLBOOK

[SPELL 6]
DEFNAME=s_night_sight
NAME=Night Sight
RUNES=IL
FLAGS=0x0204
MANAUSE=6
SKILLREQ=MAGERY 10.0
DURATION=900,2400

[SPELL 10]
DEFNAME=s_cunning
NAME=Cunning
RUNES=UW
FLAGS=0x0204
MANAUSE=4
SKILLREQ=MAGERY 20.0
DURATION=900,2400

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {READY_MARKER}|[flag_nightsight=<NIGHTSIGHT>|magery=<MAGERY>|eval_magery=<EVAL MAGERY>|sector_light=<SECTOR.LIGHT>|near_light=<ISNEARTYPE(t_light_lit,2)>]
RETURN 0
ON=@SpellCast
SYSMESSAGE {CAST_MARKER}|[flag_nightsight=<NIGHTSIGHT>|spell=<ARGN1>|difficulty=<ARGN2>|sector_light=<SECTOR.LIGHT>|near_light=<ISNEARTYPE(t_light_lit,2)>]
RETURN 0
"""
    tables.write_text(text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID={CHAR_SERIAL}
LASTCHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic mage-cast fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic mage-cast fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
NAME=MageCastProbeCharacter
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=10000
MAXMANA=10000
STAM=100
MAXSTAM=100
MAGERY=300
P=128,128,0
[WORLDITEM SYNTHETIC_SPELLBOOK]
SERIAL={BOOK_SERIAL}
CONT={PACK_SERIAL}
P=40,40
MORE1=0xFFFFFFFF
MORE2=0xFFFFFFFF
[WORLDITEM 0x0E75]
SERIAL={PACK_SERIAL}
CONT={CHAR_SERIAL}
LAYER=21
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=SPELLBOOK_ID)
    write_equipment_tile(output / "muls" / "tiledata.mul", SPELLBOOK_ID, 1)
    return 0


MODE = register_mode(
    FixtureMode(
        name="mage-cast",
        fixture_args=(),
        order=175,
        id_block=122,
        case=FixtureCase(
            name="mage-cast",
            mode="mage-cast",
            tests=(TestCase("test_mage_cast.py", (), True, True),),
            ports={"native": 3170, "asan": 3171},
            output="mage-cast",
        ),
    )
)
