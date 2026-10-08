"""Registered fixture for source-item potion spell-effect callbacks."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "PotionEffectProbe"
PASSWORD = "pfx-pw"
EVENT_NAME = "e_PotionEffectProbe"
MARKER = "SPHERE_POTION_EFFECT"
READY_MARKER = MARKER + "_READY"
ITEM_ID = 0x0E91


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[TYPEDEF 22]
DEFNAME=T_POTION

[SPELL 6]
DEFNAME=s_night_sight
NAME=synthetic night sight

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_POTION_EFFECT
NAME=synthetic night sight potion
TYPE=T_POTION
TDATA1=0
ON=@PotionEffect
SRC.SYSMESSAGE {MARKER}
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM SYNTHETIC_POTION_EFFECT
LASTNEW.MORE1=6
LASTNEW.MORE2=1000
LASTNEW.P=<SRC.P>
SRC.SYSMESSAGE {READY_MARKER} <LASTNEW.SERIAL>
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic potion-effect fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic potion-effect fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=PotionEffectProbeCharacter
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="potion-effect",
        fixture_args=(),
        order=175,
        id_block=122,
        case=FixtureCase(
            name="potion-effect",
            mode="potion-effect",
            tests=(TestCase("test_potion_effect.py", (), True, True),),
            ports={"native": 3156, "asan": 3157},
            output="potion-effect",
        ),
    )
)
