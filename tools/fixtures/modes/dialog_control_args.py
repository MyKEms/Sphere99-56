"""Synthetic gump controls whose numeric fields need Sphere evaluation."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DialogControlArgsProbe"
PASSWORD = "dca-args-pw"
EVENT_NAME = "e_DialogControlArgsProbe"
DIALOG_NAME = "d_synthetic_control_args"


def generate(output: Path) -> int:
    """Build a self-contained account and login dialog on the base fixture."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    table_text = tables.read_text(encoding="ascii")
    login_marker = "[EVENTS e_AllPlayers]\nON=@LogIn\n"
    if login_marker not in table_text:
        raise RuntimeError("base fixture has no e_AllPlayers login section")
    table_text = table_text.replace(
        login_marker, login_marker + f"DIALOG({DIALOG_NAME})\n", 1
    )
    table_text += f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
DIALOG({DIALOG_NAME})
RETURN 0
ON=@Logout
RETURN 0

[DIALOG {DIALOG_NAME}]
0 0
gumppic 30 210 03182
f_dialog_control_args_nested
argo.button(25,395,0fa5,0fa7,1,0,1)
argo.gumppic(50,420,10+5)

[FUNCTION f_dialog_control_args_nested]
button 160 330 0988 0988 0 1 0
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
    header = "TITLE=Sphere synthetic dialog control arguments\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(header + "[EOF]\n", encoding="ascii")
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL=3
ACCOUNT={ACCOUNT}
NAME=DialogControlArgsProbeCharacter
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
        name="dialog-control-args",
        fixture_args=(),
        order=158,
        id_block=104,
        case=FixtureCase(
            name="dialog-control-args",
            mode="dialog-control-args",
            tests=(TestCase("test_dialog_control_args.py", (), True, True),),
            ports={"native": 3140, "asan": 3141},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="dialog-control-args",
            test_args_by_variant={},
        ),
    )
)
