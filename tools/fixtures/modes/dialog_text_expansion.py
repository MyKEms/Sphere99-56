"""Synthetic dialog text fixture for send-time ``<?...?>`` expansion."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DialogTextExpansionProbe"
PASSWORD = "dtext-pw"
CHAR_SERIAL = 3
CHAR_NAME = "DialogTextProbe"
ITEM_ID = 0x0EAF
ITEM_UID = 0x40000020
ITEM_POINT = (127, 128, 0)
DIALOG_NAME = "d_synthetic_dialog_text"

EXPECTED_TEXTS = (
    f"Name: {CHAR_NAME}",
    "Sex: male",
    f"Nested: {CHAR_NAME}",
    'Markup: <BASEFONT COLOR="white">literal</BASEFONT>',
    "Object: synthetic dialog text probe",
)


def generate(output: Path) -> int:
    """Write one existing character and a double-clicked dialog item."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    text += f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_DIALOG_TEXT
NAME=synthetic dialog text probe
TYPE=T_NORMAL
ON=@UserDClick
DIALOG {DIALOG_NAME}
RETURN 1

[DIALOG {DIALOG_NAME}]
0 0
resizepic 0 0 5054 320 180
text 20 20 0 0
text 20 45 0 1
text 20 70 0 2
text 20 95 0 3
text 20 120 0 4

[DIALOG {DIALOG_NAME} TEXT]
Name: <?src.name?>
Sex: <?src.sex(male,female)?>
Nested: <?src.sex(<?src.name?>,female)?>
Markup: <BASEFONT COLOR="white">literal</BASEFONT>
Object: <?argo.name?>
"""
    tables.write_text(text, encoding="ascii")

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
    header = "TITLE=Sphere synthetic dialog text fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(
        header
        + f"""[WORLDITEM SYNTHETIC_DIALOG_TEXT]
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

    from modes.compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="dialog-text-expansion",
        fixture_args=(),
        # 105 is used by the open GM-toggle branch; reserve the next block.
        order=160,
        id_block=106,
        case=FixtureCase(
            name="dialog-text-expansion",
            mode="dialog-text-expansion",
            tests=(TestCase("test_dialog_text_expansion.py", (), True, True),),
            ports={"native": 3150, "asan": 3151},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="dialog-text-expansion",
            test_args_by_variant={},
        ),
    )
)
