"""Registered fixture for one null-source nested SRC expression."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .compat_writer import write_mul_fixture
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NullSourceEvalProbe"
PASSWORD = "nullsrcpw"
MARKER = "SPHERE_NULL_SOURCE_EVAL"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    """Start from the account-capable synthetic fixture and add one timer row."""
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    if "UNKNOWNKEYWORDREPORT=" not in ini_text:
        ini_text = ini_text.replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        )
        ini.write_text(ini_text, encoding="ascii")

    scripts = output / "scripts" / "spheretables.scp"
    text = scripts.read_text(encoding="ascii")

    # The login event creates the probe item after the client enters the world.
    # This makes its null-source timer run exactly once for the test instead of
    # accumulating callbacks while the server is starting.
    login_anchor = "NEWITEM SYNTHETIC_HAIR\n\n\nON=@EnvironChange\nRETURN"
    login_replacement = (
        "NEWITEM SYNTHETIC_HAIR\n"
        "NEWITEM SYNTHETIC_NULL_SOURCE_EVAL\n"
        "LASTNEW.P=128,128,0\n"
        "LASTNEW.TIMER=1\n\n\n"
        "ON=@EnvironChange\nRETURN"
    )
    if login_anchor not in text:
        raise ValueError("synthetic login event anchor was not found")
    text = text.replace(login_anchor, login_replacement, 1)
    text += f"""

[ITEMDEF 0x0F7A]
DEFNAME=SYNTHETIC_NULL_SOURCE_EVAL
NAME=synthetic null source evaluation
TYPE=T_NORMAL
ON=@Timer
IF (0)
FINDUID(3).SYSMESSAGE {MARKER}|wrong
ELSEIF (<strlen(<src.tag(probe_value)>)>)
FINDUID(3).SYSMESSAGE {MARKER}|[<src.tag(probe_value)>]
ENDIF
FINDUID(3).SYSMESSAGE {END_MARKER}
REMOVE
RETURN 0
"""
    scripts.write_text(text, encoding="ascii")

    # The compatibility generator has no saved account/character. Supply one
    # explicitly and keep the world empty; the login event creates the probe.
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic null source fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
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
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=\"Sphere synthetic null source fixture\"
VERSION=\"0.99z8\"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME={ACCOUNT}Character
ACCOUNT={ACCOUNT}
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
    write_mul_fixture(output, extra_item_id=0x0F7A)
    return 0


MODE = register_mode(
    FixtureMode(
        name="src-null-eval",
        fixture_args=(),
        order=211,
        id_block=157,
        case=FixtureCase(
            name="src-null-eval",
            mode="src-null-eval",
            tests=(TestCase("test_src_null_eval.py", (), True, True),),
            ports={"native": 43219, "asan": 43220},
            output="src-null-eval",
        ),
    )
)
