"""Synthetic coverage for dotted trigger dispatch on a linked object."""

from __future__ import annotations

import re
from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "LinkTriggerProbe"
LOGIN_VALUE = "link-trigger-pw"
EVENT_NAME = "e_LinkTriggerProbe"
TARGET_DEFNAME = "i_link_trigger_target"
DRIVER_DEFNAME = "i_link_trigger_driver"
EXPLICIT_TARGET_DEFNAME = "i_link_trigger_explicit_target"
TARGET_ID = 0x0EF1
DRIVER_ID = 0x0EF2
EXPLICIT_TARGET_ID = 0x0EF3
TARGET_SERIAL = 2
DRIVER_SERIAL = 1
EXPLICIT_TARGET_SERIAL = 4
TARGET_UID = 0x40000000 | TARGET_SERIAL
DRIVER_UID = 0x40000000 | DRIVER_SERIAL
EXPLICIT_TARGET_UID = 0x40000000 | EXPLICIT_TARGET_SERIAL
MARKER = "SPHERE_LINK_TRIGGER"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    text += f"""

[ITEMDEF 0x{TARGET_ID:04X}]
DEFNAME={TARGET_DEFNAME}
NAME=synthetic linked trigger target
TYPE=T_EQ_SCRIPT
LAYER=31
ON=@Timer
SRC.TAG.link_shorthand=1
SERV.B {MARKER}|target-shorthand|<SRC.SERIAL>|<SRC.TAG.link_shorthand>
RETURN 1

[ITEMDEF 0x{EXPLICIT_TARGET_ID:04X}]
DEFNAME={EXPLICIT_TARGET_DEFNAME}
NAME=synthetic explicit trigger target
TYPE=T_EQ_SCRIPT
LAYER=32
ON=@Timer
SRC.TAG.link_explicit=1
SERV.B {MARKER}|target-explicit|<SRC.SERIAL>|<SRC.TAG.link_explicit>
RETURN 1

[ITEMDEF 0x{DRIVER_ID:04X}]
DEFNAME={DRIVER_DEFNAME}
NAME=synthetic linked trigger driver
TYPE=T_EQ_SCRIPT
LAYER=30
ON=@Timer
SERV.B {MARKER}|driver
LINK.TRIGGER(TIMER)
SERV.B {MARKER}|after-shorthand|<SRC.TAG.link_shorthand>
FINDUID(0x{EXPLICIT_TARGET_UID:08X}).TRIGGER(@TIMER)
SERV.B {MARKER}|after-explicit|<SRC.TAG.link_explicit>
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|login
FINDUID(0x{DRIVER_UID:08X}).TRIGGER(TIMER)
RETURN 0
ON=@ItemTimer
RETURN 0
ON=@Logout
RETURN 0
"""
    tables.write_text(text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    ini = output / "sphere.ini"
    ini.write_text(
        ini.read_text(encoding="ascii").replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        ),
        encoding="ascii",
    )
    header = "TITLE=Sphere synthetic linked trigger fixture\nVERSION=0.99z8\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(
        header
        + "[EOF]\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=LinkTriggerProbeCharacter
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
[WORLDITEM {TARGET_DEFNAME}]
SERIAL={TARGET_SERIAL}
CONT=3
LAYER=31
[WORLDITEM {EXPLICIT_TARGET_DEFNAME}]
SERIAL={EXPLICIT_TARGET_SERIAL}
CONT=3
LAYER=32
[WORLDITEM {DRIVER_DEFNAME}]
SERIAL={DRIVER_SERIAL}
LINK={TARGET_UID}
CONT=3
LAYER=30
[EOF]
""",
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=max(TARGET_ID, DRIVER_ID, EXPLICIT_TARGET_ID))
    return 0


MODE = register_mode(
    FixtureMode(
        name="link-trigger",
        fixture_args=(),
        order=194,
        id_block=145,
        case=FixtureCase(
            name="link-trigger",
            mode="link-trigger",
            tests=(TestCase("test_link_trigger.py", (), True, True),),
            ports={"native": 3178, "asan": 3179},
            output="link-trigger",
        ),
    )
)
