"""Failing-first fixture for a dotted dialog dispatch on a TAG result."""

from __future__ import annotations

import re
from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DialogChainProbe"
LOGIN_VALUE = "dialog-chain-pw"
EVENT_NAME = "e_DialogChainProbe"
MARKER = "SPHERE_DIALOG_CHAIN"


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

[EVENTS {EVENT_NAME}]
ON=@LogIn
DIALOG d_dialog_chain_outer
RETURN 0

[DIALOG d_dialog_chain_outer]
0 0
ARGO.TAG(nested,<SRC>)
ARGO.TAG(nested).DIALOG(d_dialog_chain_inner,<ARGO.TAG(nested)>)
RETURN 0

[DIALOG d_dialog_chain_inner]
0 0
argo.text 20 20 0 0
argo.text 20 40 0 1.5
argo.htmlgumpa 20 60 120 20 "plain.dot" 0 0
button 20 60 4005 4006 1 0 7

[DIALOG d_dialog_chain_inner TEXT]
Nested dialog

[DIALOG d_dialog_chain_inner BUTTON]
ON=7
SYSMESSAGE {MARKER}|button|7
"""
    tables.write_text(text, encoding="ascii")

    ini = output / "sphere.ini"
    ini.write_text(
        ini.read_text(encoding="ascii").replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        ),
        encoding="ascii",
    )
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
    header = 'TITLE="Sphere synthetic dialog-chain fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n'
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
    return 0


MODE = register_mode(
    FixtureMode(
        name="dialog-chain",
        fixture_args=(),
        order=184,
        id_block=161,
        case=FixtureCase(
            name="dialog-chain",
            mode="dialog-chain",
            tests=(TestCase("test_dialog_chain.py", (), True, True),),
            ports={"native": 45218, "asan": 45219},
            output="dialog-chain",
        ),
    )
)
