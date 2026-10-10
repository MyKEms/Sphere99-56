"""Synthetic coverage for the script-defined ``craftmenu(skill)`` path."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "CraftMenuProbe"
LOGIN_VALUE = "craftmenu-pw"
EVENT_NAME = "e_CraftMenuProbe"
MARKER = "SPHERE_CRAFTMENU"
END_MARKER = MARKER + "_END"
ITEM_ID = 0x0E91


def generate(output: Path) -> int:
    """Build a minimal top-level source item and invoke the stock-style helper."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_SOURCE
NAME=synthetic craft source
TYPE=T_NORMAL

[FUNCTION craftmenu]
if (safe def_cm_<argv(0)>)
  src.tag(lastcm,<args>)
  dialog(d_craftmenu,<finduid(def_cm_<argv(0)>)>)
elseif (finduid(<argv(0)>).isitem)
  dialog(d_craftmenu,<finduid(<argv(0)>)>)
endif

[DIALOG d_craftmenu]
0 0
argo.tag(from,<finduid(<argv(0)>)>)
if !(safe argo.tag(from).isitem)
  src.sysmessage {MARKER}|bad-source
  return -1
endif
src.sysmessage {MARKER}|source-ok
resizepic 0 0 5054 260 120
text 20 20 0 0
button 20 60 4005 4006 1 0 7

[DIALOG d_craftmenu TEXT]
Craft menu

[DIALOG d_craftmenu BUTTON]
ON=7
SYSMESSAGE {MARKER}|button|7
RETURN 1

[FUNCTION craftmenu_refs]
// Keep the source character in a local VAR and exercise the three legacy
// object-root forms that the craft helper uses on a live script tree.
NEWITEM SYNTHETIC_CRAFT_SOURCE
VAR(craft_root,<ARGV(0)>)
VAR(craft_root).NEWITEM(SYNTHETIC_CRAFT_SOURCE)
SYSMESSAGE {MARKER}|var-newitem=<LASTNEW.SERIAL>
ARGV(0).NEWITEM(SYNTHETIC_CRAFT_SOURCE)
SYSMESSAGE {MARKER}|argv-newitem=<LASTNEW.SERIAL>
ARGV(0).Z=<ARGV(0).Z>-10
SYSMESSAGE {MARKER}|argv-z=<ARGV(0).Z>
RETURN 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|begin
SRC.craftmenu_refs(<SRC>)
NEWITEM SYNTHETIC_CRAFT_SOURCE
LASTNEW.P=<P>
VAR(def_cm_tinkering,<LASTNEW>)
craftmenu(tinkering)
SYSMESSAGE {MARKER}|opened
SYSMESSAGE {END_MARKER}
RETURN 0
""",
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
    (output / "save" / "sphereworld.scp").write_text(
        'TITLE="Sphere synthetic craftmenu fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic craftmenu fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME={ACCOUNT}
ACCOUNT={ACCOUNT}
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
P=128,128,20
[EOF]
""",
        encoding="ascii",
    )

    from modes.compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="craftmenu",
        fixture_args=(),
        order=183,
        id_block=130,
        case=FixtureCase(
            name="craftmenu",
            mode="craftmenu",
            tests=(TestCase("test_craftmenu.py", (), True, True),),
            ports={"native": 3164, "asan": 3165},
            output="craftmenu",
        ),
    )
)
