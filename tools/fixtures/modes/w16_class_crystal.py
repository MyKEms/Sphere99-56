"""Registered synthetic fixture for protected class-crystal attributes."""

from pathlib import Path

from .isbit import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ClassCrystalProbe"
PASSWORD = "crystal-pw"
EVENT_NAME = "e_ClassCrystalProbe"
ITEM_NAME = "SYNTHETIC_CLASS_CRYSTAL"
COUNTER_NAME = "SYNTHETIC_ABILITY_COUNTER"
ITEM_ID = 0x0EA3


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    # The base recipe's optional hair item is unrelated to this command and
    # would make the synthetic startup depend on an unplaced resource.
    table_text = table_text.replace("NEWITEM SYNTHETIC_HAIR\n", "")
    table_text += f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic protected crystal
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Create
ATTR=ATTR_MOVE_NEVER|ATTR_NEWBIE
ON=@UserDClick
SRC.NEWEQUIP {COUNTER_NAME}
SRC.SYSMESSAGE SPHERE_CLASS_CRYSTAL_CLICK
RETURN 1

[ITEMDEF 0x0EA4]
DEFNAME={COUNTER_NAME}
NAME=synthetic ability counter
TYPE=T_EQ_SCRIPT
LAYER=30

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {ITEM_NAME}
ARG(crystal_uid,<LASTNEW.UID>)
ARG(crystal_attr,<LASTNEW.ATTR>)
ARG(crystal_type,<LASTNEW.TYPE>)
ARG(click_result,<LASTNEW.TRIGGER(@USERDCLICK)>)
ARG(counter_after,<FINDID({COUNTER_NAME})>)
SYSMESSAGE SPHERE_CLASS_CRYSTAL attr=<ARG(crystal_attr)> type=<ARG(crystal_type)> click=<ARG(click_result)> counter=<ARG(counter_after)>
SYSMESSAGE SPHERE_CLASS_CRYSTAL_END
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
        name="class-crystal",
        fixture_args=None,
        order=185,
        id_block=130,
        case=FixtureCase(
            name="class-crystal",
            mode=None,
            tests=(TestCase("test_class_crystal.py", (), True, True),),
            ports={"native": 4632, "asan": 4633},
            generator="make_class_crystal_fixture.py",
            output="class-crystal",
        ),
    )
)
