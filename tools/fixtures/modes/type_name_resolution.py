"""Registered synthetic fixture for legacy type/resource aliases."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ITEM_ID = 0x0EA0
ACCOUNT = "TypeNameResolutionProbe"
PASSWORD = "type-pw"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

; These aliases precede their target TYPEDEF, matching the legacy script order.
[DEFNAMES TYPE_NAME_ALIASES]
T_EERIE_STUFF T_JUNK
T_MAGIC T_JUNK
RANDOM_REAGENT_NECRO {{ SYNTHETIC_REAGENT_A 2 SYNTHETIC_REAGENT_B 2 }}

[TYPEDEFS]
T_JUNK 85

[ITEMDEF 0x0EA1]
DEFNAME=SYNTHETIC_REAGENT_A
NAME=synthetic reagent A

[ITEMDEF 0x0EA2]
DEFNAME=SYNTHETIC_REAGENT_B
NAME=synthetic reagent B

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_TYPE_NAME_ITEM
NAME=synthetic legacy type alias item
TYPE=T_EERIE_STUFF
RESOURCES=T_MAGIC

[CHARDEF c_orc]
DEFNAME=SYNTHETIC_RESOURCE_LIST_CHAR
RESOURCES=10 RANDOM_REAGENT_NECRO
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic type-name resolution fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic type-name resolution fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=TypeNameResolutionProbe
ACCOUNT={ACCOUNT}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDITEM SYNTHETIC_TYPE_NAME_ITEM]
SERIAL=4
P=129,128,0
TYPE=T_MAGIC
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="type-name-resolution",
        fixture_args=(),
        order=115,
        id_block=115,
        case=FixtureCase(
            name="type-name-resolution",
            mode="type-name-resolution",
            tests=(TestCase("test_type_name_resolution.py", (), True, True),),
            ports={"native": 4596, "asan": 4597},
            output="type-name-resolution",
        ),
    )
)
