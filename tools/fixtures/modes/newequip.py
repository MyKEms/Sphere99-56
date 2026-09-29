"""Registered synthetic fixture mode: the legacy NEWEQUIP command."""

from pathlib import Path

from .isbit import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NewEquipProbe"
PASSWORD = "newequip-pw"
EVENT_NAME = "e_NewEquipProbe"
ITEM_NAME = "SYNTHETIC_NEWEQUIP_ITEM"
ITEM_ID = 0x0E90


def generate(output: Path) -> int:
    """Start from the common account fixture, then add only this probe."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    # The base recipe's optional hair item is unrelated to this command and
    # would make the invalid-definition assertion ambiguous.
    table_text = table_text.replace("NEWITEM SYNTHETIC_HAIR\n", "")
    table_text += f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic newequip item
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Equip
SRC.SYSMESSAGE SPHERE_NEWEQUIP_EQUIP

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWEQUIP {ITEM_NAME}
SYSMESSAGE SPHERE_NEWEQUIP_VALID <LASTNEW.SERIAL>|<SRC.FINDLAYER(30).SERIAL>
NEWEQUIP SYNTHETIC_NEWEQUIP_MISSING
SYSMESSAGE SPHERE_NEWEQUIP_INVALID <SRC.FINDLAYER(30).SERIAL>
SYSMESSAGE SPHERE_NEWEQUIP_END
RETURN 0
ON=@Logout
RETURN 0
"""
    tables.write_text(table_text, encoding="ascii")

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    char_text = char_text.replace("IsBitProbe", ACCOUNT).replace("e_AllPlayers", EVENT_NAME)
    chars.write_text(char_text, encoding="ascii")

    accounts = output / "accounts" / "sphereaccu.scp"
    account_text = accounts.read_text(encoding="ascii")
    account_text = account_text.replace("IsBitProbe", ACCOUNT).replace("isbit-pw", PASSWORD)
    accounts.write_text(account_text, encoding="ascii")
    return 0


MODE = register_mode(
    FixtureMode(
        name="newequip",
        fixture_args=None,
        order=71,
        id_block=71,
        case=FixtureCase(
            name="newequip",
            mode=None,
            tests=(TestCase("test_newequip.py", (), True, True),),
            ports={"native": 2870, "asan": 2871},
            generator="make_newequip_fixture.py",
            output="newequip",
        ),
    )
)
