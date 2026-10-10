"""Registered fixture for named non-visible effect layers."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_equipment_tile
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NightSightLayerProbe"
PASSWORD = "ns-pw"
EVENT_NAME = "e_NightSightLayerProbe"
MARKER = "SPHERE_NIGHT_SIGHT_LAYER"
END_MARKER = MARKER + "_END"
ITEM_ID = 0x0E93
LAYER_VALUE = 124


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES synthetic_named_layer]
named_internal_effect_layer {LAYER_VALUE}

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_NIGHT_SIGHT_EFFECT
NAME=synthetic Night Sight effect
TYPE=T_EQ_SCRIPT
WEIGHT=10
LAYER=named_internal_effect_layer
ON=@Equip
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
VAR weight_before,<WEIGHT>
NEWITEM SYNTHETIC_NIGHT_SIGHT_EFFECT
LASTNEW.ATTR=ATTR_DECAY
LASTNEW.TIMER=30
EQUIP(<LASTNEW>)
SYSMESSAGE {MARKER} [<LASTNEW.LAYER>|<LASTNEW.CONT.SERIAL>|<VAR(weight_before)>|<WEIGHT>]
SYSMESSAGE {END_MARKER}
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
        'TITLE="Sphere synthetic named layer fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic named layer fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=NightSightLayerProbeCharacter
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
    # Keep this item in the generated tiledata so the item-definition path is
    # exercised against a real record rather than a missing display id.
    write_equipment_tile(output / "muls" / "tiledata.mul", ITEM_ID, 0)
    return 0


MODE = register_mode(
    FixtureMode(
        name="night-sight-layer",
        fixture_args=(),
        order=187,
        id_block=133,
        case=FixtureCase(
            name="night-sight-layer",
            mode="night-sight-layer",
            tests=(TestCase("test_night_sight_layer.py", (), True, True),),
            ports={"native": 3180, "asan": 3181},
            output="night-sight-layer",
        ),
    )
)
