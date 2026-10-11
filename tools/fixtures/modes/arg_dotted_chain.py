"""Fixture for multi-hop method calls rooted in an ARG object local."""

from pathlib import Path
import re

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ArgDottedChainProbe"
LOGIN_VALUE = "arg-chain-pw"
EVENT_NAME = "e_ArgDottedChainProbe"
MARKER = "SPHERE_ARG_DOTTED_CHAIN"


def generate(output: Path) -> int:
    """Generate a minimal character whose login runs the ARG chain probe."""

    from .base import MODE as BASE_MODE

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # The base recipe's broad login event would add unrelated messages to this
    # focused parser probe.
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
F_ARG_DOTTED_CHAIN
RETURN 0
ON=@Logout
RETURN 0

[FUNCTION F_ARG_DOTTED_CHAIN]
ARG(target,<SRC>)
SYSMESSAGE {MARKER} root|[<ARG(target).NAME>]
ARG(target).SECTOR.ALLCHARS(f_arg_dotted_chain_callback)
SYSMESSAGE {MARKER} return
SYSMESSAGE {MARKER} get|[<ARG(target).NAME>]
SYSMESSAGE {MARKER}_END
RETURN 0

[FUNCTION f_arg_dotted_chain_callback]
SYSMESSAGE {MARKER} callback|[<UID>]
RETURN 1
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
    header = "TITLE=Sphere synthetic ARG dotted chain fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=ArgDottedChainProbeCharacter
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
[WORLDCHAR c_Observer]
SERIAL=4
NAME=ArgDottedChainObserver
STR=80
INT=80
DEX=80
HITS=80
MAXHITS=80
MANA=80
MAXMANA=80
STAM=80
MAXSTAM=80
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="arg-dotted-chain",
        fixture_args=(),
        order=191,
        id_block=141,
        case=FixtureCase(
            name="arg-dotted-chain",
            mode="arg-dotted-chain",
            tests=(TestCase("test_arg_dotted_chain.py", (), True, True),),
            ports={"native": 3172, "asan": 3173},
            output="arg-dotted-chain",
        ),
    )
)
