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
CONTAINER_ID = 0x0E98


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

[TYPEDEFS]
T_CONTAINER 1

[ITEMDEF 0x{CONTAINER_ID:04X}]
DEFNAME=SYNTHETIC_CRAFT_CONTAINER
NAME=synthetic craft container
TYPE=T_CONTAINER
TDATA2=1

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
NEWITEM SYNTHETIC_CRAFT_CONTAINER
ARG(indexed_root,<LASTNEW>)
SRC.TAG(indexed[0],<ARG(indexed_root)>)
DIALOG d_indexed_dispatch
RETURN 1

[DIALOG d_indexed_dispatch]
0 0
ARGO.TAG(indexed[0]).DIALOG(d_indexed_target,<ARGO.TAG(indexed[0])>)
RETURN 0

[DIALOG d_indexed_target]
0 0
text 20 20 0 0
button 20 60 4005 4006 1 0 9

[DIALOG d_indexed_target TEXT]
Indexed craft section

[FUNCTION craftmenu_refs]
// Mirror the legacy helper chain: a function receives a live object
// argument and its nested helper uses ARGV(0) as the object root.
F_craftmenu_build(<UID>)
RETURN 0

[FUNCTION f_craftmenu_build]
NEWITEM SYNTHETIC_CRAFT_SOURCE
VAR(craft_root,<ARGV(0)>)
VAR(craft_root).NEWITEM(SYNTHETIC_CRAFT_SOURCE)
SYSMESSAGE {MARKER}|var-newitem=<LASTNEW.SERIAL>
TAG(wallsCount,1)
ARG(n,0)
WHILE (<TAG(wallsCount)> > <ARG(n)>)
ARG(craft_root,<LASTNEW>)
ARG(craft_root).P=<P>
SYSMESSAGE {MARKER}|arg-p=<ARG(craft_root).P>
TAG(wallID,SYNTHETIC_CRAFT_SOURCE)
ARGV(0).NEWITEM(<TAG(wallID)>)
SYSMESSAGE {MARKER}|argv-newitem=<LASTNEW.SERIAL>
ARG(wall_root,<LASTNEW>)
ARG(wall_root).P=<P>
F_craftmenu_arg_property(<ARG(wall_root)>)

// Craft dialogs enumerate a container with a local loop index:
// `findcont(icount)`. Keep two children so a raw argument that falls back to
// zero cannot satisfy this regression row.
NEWITEM SYNTHETIC_CRAFT_CONTAINER
ARG(craft_container,<LASTNEW>)
NEWITEM SYNTHETIC_CRAFT_SOURCE
LASTNEW.CONT=<ARG(craft_container)>
ARG(craft_first,<LASTNEW>)
NEWITEM SYNTHETIC_CRAFT_SOURCE
LASTNEW.CONT=<ARG(craft_container)>
ARG(craft_second,<LASTNEW>)
ARG(index,1)
SYSMESSAGE {MARKER}|findcont-local=<ARG(craft_container).FINDCONT(index).SERIAL>|want=<ARG(craft_first).SERIAL>|other=<ARG(craft_second).SERIAL>
ARG(n,#+1)
ENDWHILE
RETURN 0

[FUNCTION f_craftmenu_arg_property]
ARGV(0).Z=<ARGV(0).Z>-10
SYSMESSAGE {MARKER}|argv-z=<ARGV(0).Z>

[FUNCTION f_craftmenu_negated_conditions]
// A leading negation only wraps the whole condition when its parenthesis
// closes at the end; otherwise it is one operand of a compound condition.
ARG(a,0)
ARG(b,0)
IF !(<ARG(a)>) && (<ARG(b)>)
  SYSMESSAGE {MARKER}|neg-and=1
ELSE
  SYSMESSAGE {MARKER}|neg-and=0
ENDIF
ARG(a,1)
ARG(b,1)
IF !(<ARG(a)>) || (<ARG(b)>)
  SYSMESSAGE {MARKER}|neg-or=1
ELSE
  SYSMESSAGE {MARKER}|neg-or=0
ENDIF
IF !(ISGM) && (NPC)
  SYSMESSAGE {MARKER}|neg-npc=1
ELSE
  SYSMESSAGE {MARKER}|neg-npc=0
ENDIF
IF !((<ARG(a)>) && (<ARG(b)>))
  SYSMESSAGE {MARKER}|neg-whole=1
ELSE
  SYSMESSAGE {MARKER}|neg-whole=0
ENDIF

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|begin
F_craftmenu_negated_conditions
F_craftmenu_build(<UID>)
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
    sphere_ini = output / "sphere.ini"
    sphere_ini.write_text(
        sphere_ini.read_text(encoding="ascii").replace(
            "DEBUGLEVEL=0\n",
            "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
            1,
        ),
        encoding="ascii",
    )
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
        id_block=138,
        case=FixtureCase(
            name="craftmenu",
            mode="craftmenu",
            tests=(TestCase("test_craftmenu.py", (), True, True),),
            ports={"native": 3164, "asan": 3165},
            output="craftmenu",
        ),
    )
)
