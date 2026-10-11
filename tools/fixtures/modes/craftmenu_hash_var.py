"""Synthetic coverage for persisted hash-serial craft-menu references."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "CraftHashProbe"
LOGIN_VALUE = "craft-hash-pw"
EVENT_NAME = "e_CraftHashVarProbe"
MARKER = "SPHERE_CRAFT_HASH_VAR"
END_MARKER = MARKER + "|end"
ITEM_ID = 0x0EB5


def generate(output: Path) -> int:
    """Build a minimal craft-menu helper whose VAR value uses ``#<serial>``."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_HASH_SOURCE
NAME=synthetic craft hash source
TYPE=T_NORMAL

[FUNCTION craftmenu]
if (safe def_cm_<argv(0)>)
  src.sysmessage {MARKER}|root-ok
  if (finduid(def_cm_<argv(0)>).isitem)
    src.sysmessage {MARKER}|item-ok
  endif
  dialog(d_craft_hash_menu,<VAR(def_cm_<argv(0)>)>)
elseif (finduid(<argv(0)>).isitem)
  dialog(d_craft_hash_menu,<finduid(<argv(0)>)>)
endif

[DIALOG d_craft_hash_menu]
0 0
argo.tag(from,<VAR(def_cm_<argv(0)>)>)
if !(safe argo.tag(from).isitem)
  src.sysmessage {MARKER}|bad-source
  return -1
endif
src.sysmessage {MARKER}|source-ok
resizepic 0 0 5054 260 120
text 20 20 0 0
button 20 60 4005 4006 1 0 7

[DIALOG d_craft_hash_menu TEXT]
Craft hash menu

[DIALOG d_craft_hash_menu BUTTON]
ON=7
SYSMESSAGE {MARKER}|button|7
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|begin
NEWITEM SYNTHETIC_CRAFT_HASH_SOURCE
LASTNEW.P=<P>
VAR(def_cm_tinkering,#<LASTNEW.SERIAL>)
SYSMESSAGE {MARKER}|stored|[<VAR(def_cm_tinkering)>]
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
        'TITLE="Sphere synthetic craft hash VAR fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic craft hash VAR fixture"
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
        name="craftmenu-hash-var",
        fixture_args=(),
        order=184,
        id_block=131,
        case=FixtureCase(
            name="craftmenu-hash-var",
            mode="craftmenu-hash-var",
            tests=(TestCase("test_craftmenu_hash_var.py", (), True, True),),
            ports={"native": 3172, "asan": 3173},
            generator="make_fixture.py",
            output="craftmenu-hash-var",
        ),
    )
)
