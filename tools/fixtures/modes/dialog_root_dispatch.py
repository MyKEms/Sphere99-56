"""Registered synthetic fixture mode: script functions called on a dialog.

A double-clicked item assigns a reference-valued LINK and opens a dialog on
that linked object.  The layout writes one gump
control directly and builds the rest through script functions called on the
dialog object, ``argo.<function>(...)``.  Those functions set a TAG on their
base object, emit controls and texts both directly and through ARGO, and call
a further function on ARGO, as a layout written with helper functions does.
The dialog's button runs a function on ARGO that opens a second dialog whose
texts are larger than one output buffer; that dialog's button opens a third
one that does not fit in a single packet at all.

tools/fixtures/test_dialog_root_dispatch.py decodes every 0xB0 packet and
checks the stream stays in step after the replies.
"""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DialogRootProbe"
PASSWORD = "droot-pw"
CHAR_SERIAL = 3
MARKER = "SPHERE_DIALOG_ROOT"

# The item whose double-click resolves a reference-valued LINK before opening
# the first dialog on the linked object.
ITEM_ID = 0x0EAC
ITEM_NAME = "SYNTHETIC_DIALOG_ROOT"
ITEM_UID = 0x40000020
TARGET_UID = 0x40000021
ITEM_POINT = (127, 128, 0)
TARGET_POINT = (126, 128, 0)

DIALOG_NAME = "d_synthetic_root_dispatch"
NEXT_DIALOG_NAME = "d_synthetic_root_dispatch_next"
OVERSIZE_DIALOG_NAME = "d_synthetic_root_dispatch_oversize"
FIRST_BUTTON = 41
NEXT_BUTTON = 42
MISSING_FUNCTION = "f_dialog_root_missing"

# A text line longer than one conversion buffer (1024 characters).
LONG_TEXT = "long line " + "0123456789" * 150
# Bulk text lines: the next dialog is larger than the server's output buffer,
# the oversize one larger than the 16-bit 0xB0 length field.
BULK_LINE_COUNT = 24
OVERSIZE_LINE_COUNT = 40


def bulk_line(index: int) -> str:
    return f"bulk line {index:02d} " + "abcdefghij" * 90


# The first dialog: one control written in the layout, the rest from the
# functions, then one control that reads the TAG the first function set.
PANEL_Y = 3 + 20
FIRST_CONTROLS = (
    "gumppic 140 200 2200",
    "gumppictiled 10 3 300 200 2624",
    "gumppic 10 8 2201",
    f"htmlgump 10 {PANEL_Y} 280 40 1 0 0",
    f"button 40 150 4005 4007 1 0 {FIRST_BUTTON}",
    "text 80 150 0 2",
    "htmlgump 10 70 280 40 5 0 0",
    "htmlgump 10 120 280 40 6 0 0",
    f"text 200 {PANEL_Y} 0 3",
)
FIRST_TEXTS = (
    LONG_TEXT,
    "300 wide panel",
    "label 80",
    "tag text",
    "note 300",
    '<BASEFONT COLOR="white">Inline "quoted" HTML</BASEFONT>',
    '<BASEFONT COLOR="silver">ARGO inline text</BASEFONT>',
)
NEXT_LABEL_TEXT_ID = BULK_LINE_COUNT
NEXT_CONTROLS = (
    "resizepic 0 0 5054 420 400",
    "htmlgump 20 20 380 320 0 1 1",
    f"button 20 360 4005 4007 1 0 {NEXT_BUTTON}",
    f"text 60 360 0 {NEXT_LABEL_TEXT_ID}",
)
NEXT_TEXTS = tuple(bulk_line(index) for index in range(BULK_LINE_COUNT)) + ("label 60",)


def scripts() -> str:
    """Return the item, dialogs and functions of the probe."""

    bulk = "\n".join(bulk_line(index) for index in range(BULK_LINE_COUNT))
    oversize = "\n".join(bulk_line(index) for index in range(OVERSIZE_LINE_COUNT))
    return f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic dialog root
TYPE=T_SYNTHETIC_DIALOG_ROOT

[TYPEDEF 1100]
DEFNAME=T_SYNTHETIC_DIALOG_ROOT
ON=@UserDClick
LINK=<FINDUID(0x{TARGET_UID:08X})>
F_DIALOG_ROOT_LINK
RETURN 1

[FUNCTION F_DIALOG_ROOT_LINK]
LINK.DIALOG {DIALOG_NAME}

[DIALOG {DIALOG_NAME}]
0 0
gumppic 140 200 2200
argo.f_dialog_root_panel(10,3,300,200)
argo.f_dialog_root_button(40,150,{FIRST_BUTTON},2)
argo.f_dialog_root_deep(0)
HTMLGUMPa 10 70 280 40 "<BASEFONT COLOR=\\"white\\">Inline \\"quoted\\" HTML</BASEFONT>" 0 0
argo.HTMLGumpa(10,120,280,40,"<BASEFONT COLOR=\\"silver\\">ARGO inline text</BASEFONT>",0,0)
text 200 <tag(dialog_root_y[1])> 0 3

[DIALOG {DIALOG_NAME} TEXT]
{LONG_TEXT}
text one placeholder
text two placeholder
tag text

[DIALOG {DIALOG_NAME} BUTTON]
ON={FIRST_BUTTON}
argo.f_dialog_root_open(<ARGN>)

[DIALOG {NEXT_DIALOG_NAME}]
0 0
resizepic 0 0 5054 420 400
htmlgump 20 20 380 320 0 1 1
argo.f_dialog_root_button(20,360,{NEXT_BUTTON},{NEXT_LABEL_TEXT_ID})

[DIALOG {NEXT_DIALOG_NAME} TEXT]
{bulk}

[DIALOG {NEXT_DIALOG_NAME} BUTTON]
ON={NEXT_BUTTON}
DIALOG {OVERSIZE_DIALOG_NAME}
SRC.SYSMESSAGE {MARKER} oversize_after

[DIALOG {OVERSIZE_DIALOG_NAME}]
0 0
resizepic 0 0 5054 420 400
htmlgump 20 20 380 320 0 1 1

[DIALOG {OVERSIZE_DIALOG_NAME} TEXT]
{oversize}

[FUNCTION f_dialog_root_panel]
// x,y,width,height: a TAG on the base, controls written directly and in call
// form, one control and one text through ARGO, and a function whose name
// starts with a control name.
tag(dialog_root_y[1],<eval <argv(1)>+20>)
gumppictiled <argv(0)> <argv(1)> <argv(2)> <argv(3)> 2624
gumppic(<argv(0)>,<eval <argv(1)>+5>,2201)
argo.htmlgump(<argv(0)>,<argo.tag(dialog_root_y[1])>,280,40,1,0,0)
argo.settext(1,<argv(2)> wide panel)
textline_note(<argv(2)>)
argo.{MISSING_FUNCTION}(1)

[FUNCTION textline_note]
settext(4,note <argv(0)>)

[FUNCTION f_dialog_root_button]
// x,y,button,text id: a button and a label from a further function.
argo.button(<argv(0)>,<argv(1)>,4005,4007,1,0,<argv(2)>)
argo.f_dialog_root_label(<eval <argv(0)>+40>,<argv(1)>,<argv(3)>)

[FUNCTION f_dialog_root_label]
argo.text(<argv(0)>,<argv(1)>,0,<argv(2)>)
argo.settext(<argv(2)>,label <argv(0)>)

[FUNCTION f_dialog_root_deep]
// Calls itself without an exit: the recursion guard must end it.
argo.f_dialog_root_deep(<eval <argv(0)>+1>)

[FUNCTION f_dialog_root_open]
SRC.SYSMESSAGE {MARKER} button|<argv(0)>|<tag(dialog_root_y[1])>
DIALOG {NEXT_DIALOG_NAME}
"""


def generate(output: Path) -> int:
    """Generate one existing character next to the dialog item."""

    from modes.compat_writer import write_mul_fixture

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    # The common login hook prints unrelated markers and reports its own
    # unknown keywords.  This mode checks the keyword report, so the login
    # stays quiet.
    text, count = re.subn(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    if count != 1:
        raise RuntimeError("synthetic login event was not found exactly once")
    tables.write_text(text + scripts(), encoding="ascii")

    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    ini.write_text(
        ini_text.replace(
            "[SPHERE]\n",
            "[SPHERE]\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        ),
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
    x, y, z = ITEM_POINT
    # Both files of a save carry the same SAVECOUNT header.
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic dialog root fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM {ITEM_NAME}]
SERIAL=0{ITEM_UID:x}
P={x},{y},{z}
[WORLDITEM {ITEM_NAME}]
SERIAL=0{TARGET_UID:x}
P={TARGET_POINT[0]},{TARGET_POINT[1]},{TARGET_POINT[2]}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic dialog root fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME=DialogRootProbe
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
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="dialog-root-dispatch",
        fixture_args=(),
        # Next free block after current master.  Recheck the registry before
        # adding another mode.
        order=80,
        id_block=80,
        case=FixtureCase(
            name="dialog-root-dispatch",
            mode="dialog-root-dispatch",
            tests=(TestCase("test_dialog_root_dispatch.py", (), True, True),),
            ports={"native": 2886, "asan": 2887},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="dialog-root-dispatch",
            test_args_by_variant={},
        ),
    )
)
