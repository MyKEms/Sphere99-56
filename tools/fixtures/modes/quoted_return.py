"""Failing-first fixture for a quoted deferred numeric trigger return."""

from __future__ import annotations

import re
from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "QuotedReturnProbe"
LOGIN_VALUE = "quoted-return-pw"
EVENT_NAME = "e_QuotedReturnProbe"
ITEM_ID = 0x0EAD
ITEM_NAME = "SYNTHETIC_QUOTED_RETURN_ITEM"
DIALOG_ITEM_ID = 0x0EAE
DIALOG_ITEM_NAME = "SYNTHETIC_QUOTED_RETURN_DIALOG_ITEM"
DIALOG_NAME = "d_synthetic_quoted_return"
MARKER = "SPHERE_QUOTED_RETURN"
READY_MARKER = MARKER + " READY"
HANDLED_MARKER = MARKER + " HANDLED"
BASE_MARKER = MARKER + " BASE"
DIALOG_HANDLED_MARKER = MARKER + " DIALOG_HANDLED"
BYTE_PACKET = "02 00 00 00 00 00 00"


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

[TYPEDEF 1101]
DEFNAME=T_SYNTHETIC_QUOTED_RETURN
ON=@UserDClick
SRC.SYSMESSAGE <f_quoted_byte_packet_deferred>
RETURN \"<f_quoted_return_status>\"

[FUNCTION f_quoted_return_status]
SRC.SYSMESSAGE {HANDLED_MARKER}
RETURN 1

[FUNCTION f_quoted_byte_packet]
RETURN "{BYTE_PACKET}"

[FUNCTION f_quoted_byte_packet_deferred]
RETURN "<f_quoted_byte_packet>"

[TYPEDEF 1102]
DEFNAME=T_SYNTHETIC_QUOTED_RETURN_DIALOG
ON=@UserDClick
DIALOG {DIALOG_NAME}
RETURN 1

[FUNCTION f_quoted_dialog_status]
SRC.SYSMESSAGE {DIALOG_HANDLED_MARKER}
RETURN 1

[DIALOG {DIALOG_NAME}]
0 0
button 20 20 4005 4007 1 0 2

[DIALOG {DIALOG_NAME} BUTTON]
ON=2
RETURN "<f_quoted_dialog_status>"

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic quoted-return item
TYPE=T_SYNTHETIC_QUOTED_RETURN
ON=@UserDClick
SRC.SYSMESSAGE {BASE_MARKER}
RETURN 0

[ITEMDEF 0x{DIALOG_ITEM_ID:04X}]
DEFNAME={DIALOG_ITEM_NAME}
NAME=synthetic quoted-return dialog item
TYPE=T_SYNTHETIC_QUOTED_RETURN_DIALOG

[EVENTS {EVENT_NAME}]
ON=@LogIn
NEWITEM {ITEM_NAME}
LASTNEW.P=128,128,0
ARG(item_uid,<LASTNEW.SERIAL>)
NEWITEM {DIALOG_ITEM_NAME}
LASTNEW.P=129,128,0
SYSMESSAGE {READY_MARKER}|<ARG(item_uid)>|<LASTNEW.SERIAL>
RETURN 0
"""
    tables.write_text(text, encoding="ascii")
    accounts = output / "accounts" / "sphereaccu.scp"
    accounts.write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_VALUE}
CHARUID=3
LASTCHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    header = 'TITLE="Sphere synthetic quoted-return fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n'
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME={ACCOUNT}
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
    from .compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=max(ITEM_ID, DIALOG_ITEM_ID))
    return 0


MODE = register_mode(
    FixtureMode(
        name="quoted-return",
        fixture_args=(),
        order=217,
        id_block=163,
        case=FixtureCase(
            name="quoted-return",
            mode="quoted-return",
            tests=(TestCase("test_quoted_return.py", (), True, True),),
            ports={"native": 45228, "asan": 45229},
            output="quoted-return",
        ),
    )
)
