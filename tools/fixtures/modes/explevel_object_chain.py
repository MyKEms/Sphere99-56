"""Registered fixture for object-valued TAG dotted chains."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ExplevelObjectChainProbe"
PASSWORD = "eoc-pw"
EVENT_NAME = "e_ExplevelObjectChainProbe"
MARKER = "SPHERE_EXPLEVEL_OBJECT_CHAIN"
END_MARKER = MARKER + "_END"
ITEM_ID = 0x0EF0


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_EXPLEVEL_WEAPON
NAME=synthetic explevel weapon
TYPE=T_EQ_SCRIPT
LAYER=30

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM SYNTHETIC_EXPLEVEL_WEAPON
EQUIPLAST
TAG(weaponuid,<SRC.FINDLAYER(30)>)
SYSMESSAGE {MARKER} [<tag(weaponuid)>|<ISUIDVALID <tag(weaponuid)>>|<FINDUID(<tag(weaponuid)>).name>|<tag(weaponuid).serial>|<tag(weaponuid).name>|<tag(weaponuid).type>|<tag(weaponuid).typedef.dispid>]
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
        'TITLE="Sphere synthetic explevel object-chain fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic explevel object-chain fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ExplevelObjectChainProbeCharacter
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
        name="explevel-object-chain",
        fixture_args=(),
        order=178,
        id_block=125,
        case=FixtureCase(
            name="explevel-object-chain",
            mode="explevel-object-chain",
            tests=(TestCase("test_explevel_object_chain.py", (), True, True),),
            ports={"native": 4600, "asan": 4601},
            output="explevel-object-chain",
        ),
    )
)
