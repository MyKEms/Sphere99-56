"""Synthetic character status-flag property reads and writes."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "FlagProp209"
PASSWORD = "flag209-pw"
EVENT_NAME = "e_FlagProperties209Probe"
MARKER = "SPHERE_FLAG_PROPERTIES_209"

FLAGS = (
    "CONJURED",
    "CRIMINAL",
    "DEAD",
    "FREEZE",
    "HASSHIELD",
    "HIDDEN",
    "IMMOBILE",
    "INCOGNITO",
    "INSUBSTANTIAL",
    "INVISIBLE",
    "INVUL",
    "NIGHTSIGHT",
    "ONHORSE",
    "PET",
    "POISONED",
    "POLYMORPHED",
    "REACTIVE",
    "REFLECTION",
    "RIDDEN",
    "SLEEPING",
    "SPAWNED",
    "STONE",
    "WAR",
)


def _values(prefix: str) -> str:
    return "|".join(f"[{prefix}{flag}>]" for flag in FLAGS)


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    # Remove the noisy all-player hook from the base recipe so the marker rows
    # identify only this focused status-flag probe.
    table_text = re.sub(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        table_text,
        count=1,
        flags=re.DOTALL,
    )
    table_text += f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} read|{_values('<FLAG_')}
FLAG_NIGHTSIGHT=1
FLAG_HIDDEN=1
FLAG_INVISIBLE=1
FLAG_WAR=1
SYSMESSAGE {MARKER} set|[<FLAG_NIGHTSIGHT>]|[<FLAG_HIDDEN>]|[<FLAG_INVISIBLE>]|[<FLAG_WAR>]
FLAG_NIGHTSIGHT=0
FLAG_HIDDEN=0
FLAG_INVISIBLE=0
FLAG_WAR=0
SYSMESSAGE {MARKER} clear|[<FLAG_NIGHTSIGHT>]|[<FLAG_HIDDEN>]|[<FLAG_INVISIBLE>]|[<FLAG_WAR>]
SYSMESSAGE {MARKER}_END
RETURN 0
ON=@Logout
RETURN 0
"""
    tables.write_text(table_text, encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    header = "TITLE=Sphere synthetic flag properties fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=FlagProperties209ProbeCharacter
EVENTS={EVENT_NAME}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="flag-properties",
        fixture_args=(),
        order=186,
        id_block=132,
        case=FixtureCase(
            name="flag-properties",
            mode="flag-properties",
            tests=(TestCase("test_flag_properties.py", (), True, True),),
            ports={"native": 5200, "asan": 5201},
            generator="make_fixture.py",
            output="flag-properties",
        ),
    )
)
