"""Registered synthetic fixture for the character starting-kit path.

The production helper creates an item, then calls ``lastnew.logcont(<uid>)``.
The latter function receives the character as ``args`` and resolves it with
``finduid(args)``.  This mode removes the character's pack before exercising
that exact sequence so the engine must both resolve the argument and recreate
the pack-safe destination.
"""

from __future__ import annotations

import re
from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "StartingKitProbe"
PASSWORD = "kit-pw"
CHAR_SERIAL = 3
ITEM_NAME = "SYNTHETIC_STARTING_KIT"
MARKER = "SPHERE_STARTING_KIT"


def _patch_character_save(output: Path) -> None:
    path = output / "save" / "spherechars.scp"
    text = path.read_text(encoding="ascii")
    # Keep the character section but remove the static content items supplied
    # by the compatibility writer.  The login script below creates the kit.
    text = re.sub(
        r"(?ms)^\[WORLDITEM .*?(?=^\[EOF\])",
        "",
        text,
    )
    text = text.replace("CharacterContentProbe", ACCOUNT)
    path.write_text(text, encoding="ascii")

    account_path = output / "accounts" / "sphereaccu.scp"
    account_text = account_path.read_text(encoding="ascii")
    account_path.write_text(
        account_text.replace("CharacterContentProbe", ACCOUNT).replace(
            "char_content_pw", PASSWORD
        ),
        encoding="ascii",
    )


def _patch_scripts(output: Path) -> None:
    path = output / "scripts" / "spheretables.scp"
    text = path.read_text(encoding="ascii")
    text = text.replace("CharacterContentProbe", ACCOUNT)
    text = text.replace("char_content_pw", PASSWORD)

    # The compatibility writer already defines this no-layer item.  Rename it
    # so the checker cannot accidentally accept a stale save section.
    text = text.replace("SYNTHETIC_CHARACTER_CONTENT", ITEM_NAME)
    # The base character-content mode also emits its own markers and starts a
    # save during login.  This probe owns the login action and deliberately
    # keeps the assertion focused on the recreated pack and generated item.
    text = re.sub(
        r"(?ms)^SYSMESSAGE SPHERE_CHARACTER_CONTENT_UID.*?^"
        r"SYSMESSAGE SPHERE_CHARACTER_CONTENT_END\n",
        "",
        text,
    )

    helper = f"""
[FUNCTION giveitem]
NEWITEM <ARGV(0)>
lastnew.logcont(<uid>)
RETURN 1

[FUNCTION logcont]
ARG(moveto,<finduid(args)>)
CONT=<ARG(moveto)>
RETURN 1

"""
    text = text.replace("[EVENTS e_AllPlayers]", helper + "[EVENTS e_AllPlayers]", 1)

    login = f"""
findlayer(layer_pack).remove
giveitem({ITEM_NAME})
SYSMESSAGE {MARKER}_CHAR <uid>
SYSMESSAGE {MARKER}_PACK <findid({ITEM_NAME}).cont.serial>
SYSMESSAGE {MARKER}_ITEM <findid({ITEM_NAME}).serial>
SYSMESSAGE {MARKER}_PARENT <findid({ITEM_NAME}).cont.serial>
SYSMESSAGE {MARKER}_TOP <findid({ITEM_NAME}).topobj.serial>
SYSMESSAGE {MARKER}_END
"""
    text = text.replace("ON=@LogIn\n", "ON=@LogIn\n" + login, 1)
    path.write_text(text, encoding="ascii")


def generate(output: Path) -> int:
    result = generate_recipe(output, MODE)
    if result:
        return result
    _patch_character_save(output)
    _patch_scripts(output)
    return 0


MODE = register_mode(
    FixtureMode(
        name="starting-kit",
        fixture_args=("--world-load-counts", "--character-content-probe"),
        order=90,
        id_block=90,
        case=FixtureCase(
            name="starting-kit",
            mode="starting-kit",
            tests=(TestCase("test_starting_kit.py", (), True, True),),
            ports={"native": 2970, "asan": 2971},
            generator="make_fixture.py",
            output="starting-kit",
        ),
    )
)
