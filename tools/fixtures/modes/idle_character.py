"""Registered synthetic fixture mode for an idle character's status ticks."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "IdleCharacterProbe"
PASSWORD = "pw"
CHARACTER_SERIAL = 3
ITEM_ID = 0x0E8D


def _script() -> str:
    return """; Synthetic idle-character status fixture.
[TYPEDEF 0]
DEFNAME=T_NORMAL

[TYPEDEF 176]
DEFNAME=T_EQ_SCRIPT

[RACECLASS 0]
DEFNAME=race_undeclared
REGEN_1=1
REGEN_2=1
REGEN_3=1
REGEN_4=1*60*24
REGEN_5=1*60*48

[ITEMDEF 0x0E8D]
DEFNAME=SYNTHETIC_IDLE_OBSERVER
NAME=synthetic idle observer
TYPE=T_EQ_SCRIPT
LAYER=31
ON=@Equip
TIMER=1
RETURN 0
ON=@Timer
CONT.SYSMESSAGE SPHERE_IDLE|<CONT.FOOD>|<CONT.HITS>|<CONT.MANA>|<CONT.STAM>
TIMER=1
RETURN 1

[CHARDEF 0x0190]
DEFNAME=c_MAN
DEFNAME2=DEFAULTCHAR
NAME=synthetic human
ID=0x0190
STR=100
INT=100
DEX=100

[EVENTS e_IDLE_PROBE]
ON=@LogIn
SRC.NEWITEM=SYNTHETIC_IDLE_OBSERVER
EQUIP(<LASTNEW>)
SRC.ACT.TIMER=1
SYSMESSAGE SPHERE_RATE_SET
RETURN 0

[EVENTS e_AllPlayers]
ON=@LogIn
RETURN 0

[SPEECH spk_AllPlayers]

[AREA Synthetic world]
P=128,128,0
RECT=1,1,6143,4096

[EOF]
"""


def generate(output: Path) -> int:
    """Generate a minimal, redistributable idle-character runtime."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    (output / "scripts" / "spheretables.scp").write_text(
        _script(), encoding="ascii"
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"[ACCOUNT {ACCOUNT}]\n"
        f"PASSWORD={PASSWORD}\n"
        f"CHARUID={CHARACTER_SERIAL}\n"
        f"LASTCHARUID={CHARACTER_SERIAL}\n"
        "[EOF]\n",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text(
        "[EOF]\n", encoding="ascii"
    )
    header = (
        'TITLE="Sphere synthetic idle-character fixture"\n'
        'VERSION="0.99z8"\n'
        "Time=1054683265\n"
        "SAVECOUNT=0\n"
    )
    (output / "save" / "sphereworld.scp").write_text(
        header + "[EOF]\n", encoding="ascii"
    )
    (output / "save" / "spherechars.scp").write_text(
        header
        + "[WORLDCHAR c_MAN]\n"
        + f"SERIAL={CHARACTER_SERIAL}\n"
        + f"NAME={ACCOUNT}\n"
        + f"ACCOUNT={ACCOUNT}\n"
        + "EVENTS=e_IDLE_PROBE\n"
        + "STR=100\nINT=100\nDEX=100\n"
        + "HITS=95\nMAXHITS=100\n"
        + "MANA=100\nMAXMANA=100\n"
        + "STAM=100\nMAXSTAM=100\n"
        + "FOOD=7\n"
        + "P=128,128,0\n"
        + "[EOF]\n",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="idle-character",
        fixture_args=(),
        order=191,
        id_block=137,
        case=FixtureCase(
            name="idle-character",
            mode="idle-character",
            tests=(TestCase("test_idle_character.py", (), True, True),),
            ports={"native": 3190, "asan": 3191},
            output="idle-character",
        ),
    )
)
