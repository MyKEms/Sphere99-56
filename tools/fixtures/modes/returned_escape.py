"""Synthetic coverage for escapes returned by a script function."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "RetEscProbe"
PASSWORD = "ret-esc-pw"
CHAR_SERIAL = 3
CHAR_NAME = "ReturnedEscapeProbe"
ITEM_ID = 0x0EB0
ITEM_UID = 0x40000021
ITEM_POINT = (127, 128, 0)
DIALOG_NAME = "d_returned_escape"
MARKER = "SPHERE_RETURNED_ESCAPE"

EXPECTED_TEXTS = (
    f"Direct: {CHAR_NAME}",
    f"Nested: {CHAR_NAME}",
)


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[FUNCTION f_returned_escape]
ARG(open,"<")
ARG(body,"SRC.NAME>")
RETURN "<arg(open)><arg(body)>"

[FUNCTION f_returned_escape_nested]
ARG(open,"<")
ARG(inner,"f_returned_escape()>")
RETURN "<arg(open)><arg(inner)>"

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_RETURNED_ESCAPE
NAME=synthetic returned-escape dialog
TYPE=T_NORMAL
ON=@UserDClick
DIALOG {DIALOG_NAME}
RETURN 1

[DIALOG {DIALOG_NAME}]
0 0
resizepic 0 0 5054 320 140
text 20 20 0 0
text 20 45 0 1

[DIALOG {DIALOG_NAME} TEXT]
Direct: <?f_returned_escape()?>
Nested: <?f_returned_escape_nested()?>

[EVENTS e_ReturnedEscapeProbe]
ON=@LogIn
SYSMESSAGE {MARKER}|direct=<?f_returned_escape()?>
SYSMESSAGE {MARKER}|nested=<?f_returned_escape_nested()?>
RETURN 0
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID={CHAR_SERIAL}
CHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    x, y, z = ITEM_POINT
    header = 'TITLE="Sphere synthetic returned-escape fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n'
    (output / "save" / "sphereworld.scp").write_text(
        header
        + f"""[WORLDITEM SYNTHETIC_RETURNED_ESCAPE]
SERIAL=0{ITEM_UID:x}
P={x},{y},{z}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME={CHAR_NAME}
EVENTS=e_ReturnedEscapeProbe
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

    from modes.compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="returned-escape",
        fixture_args=(),
        order=221,
        id_block=167,
        case=FixtureCase(
            name="returned-escape",
            mode="returned-escape",
            tests=(TestCase("test_returned_escape.py", (), True, True),),
            ports={"native": 45228, "asan": 45229},
            output="returned-escape",
        ),
    )
)
