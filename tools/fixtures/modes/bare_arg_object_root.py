"""Synthetic coverage for an object UID held by a bare ARG local."""

from pathlib import Path

from .registry import FixtureCase, FixtureMode, TestCase, register_mode
from .legacy_generator import generate as generate_recipe


ACCOUNT = "BareArgObjectRootProbe"
PASSWORD = "pw"
EVENT_NAME = "e_BareArgObjectRootProbe"
NPC_DEFNAME = "c_BareArgObjectRootProbe"
SPAWN_DEFNAME = "SYNTHETIC_BARE_ARG_SPAWN"
DRIVER_DEFNAME = "i_bare_arg_probe_timer"
MARKER = "SPHERE_BARE_ARG_ROOT"
END_MARKER = MARKER + "|end"
ITEM_ID = 0x0EB6
SPAWN_ID = ITEM_ID + 1
DRIVER_ID = ITEM_ID + 2
UID_F_ITEM = 0x40000000
SPAWN_SERIAL = UID_F_ITEM | 1
DRIVER_SERIAL = UID_F_ITEM | 2

# This module sorts before ``base`` during discovery. Keep the compatibility
# recipe argument local so importing it does not register the shared base mode
# twice.
BASE_MODE = FixtureMode(name="bare-arg-object-root-base", fixture_args=())


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[TYPEDEF 34]
DEFNAME=T_SPAWN_CHAR

[TYPEDEF 74]
DEFNAME=T_EQ_MEMORY_OBJ

[DEFNAMES bare_arg_object_root]
MEMORY_ISPAWNED 00200
STATF_SPAWNED 010000000
FLAG_SPAWNED 010000000

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME=SYNTHETIC_BARE_ARG_OBJECT
NAME=synthetic bare ARG object
TYPE=T_NORMAL

[ITEMDEF 0x{SPAWN_ID:04X}]
DEFNAME={SPAWN_DEFNAME}
NAME=synthetic bare ARG spawn
TYPE=T_SPAWN_CHAR

[ITEMDEF 0x{DRIVER_ID:04X}]
DEFNAME={DRIVER_DEFNAME}
NAME=synthetic bare ARG probe timer
TYPE=T_EQ_SCRIPT
LAYER=31
ON=@Equip
TIMER=1
ON=@Timer
SERV.B {MARKER}|driver
SRC=<CONT>
CONT.creaturestart
REMOVE
RETURN 1

[ITEMDEF 0x2007]
DEFNAME=i_memory
TYPE=T_EQ_MEMORY_OBJ
LAYER=30

[CHARDEF {NPC_DEFNAME}]
DEFNAME={NPC_DEFNAME}
NAME=synthetic bare ARG object NPC
ID=0x0190
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
CAN=0
ON=@Create
SERV.B {MARKER}|created
NEWITEM {DRIVER_DEFNAME}
EQUIPLAST
RETURN 0

[FUNCTION itemExists]
SERV.B {MARKER}|itemarg|[<ARGV(0)>]|[<FINDUID(<ARGV(0)>).ISITEM>]
IF (<ARGV(0)> == 0)
  RETURN 0
ENDIF
IF (safe finduid(<ARGV(0)>).isItem)
  RETURN 1
ENDIF
RETURN 0

[FUNCTION creaturestart]
IF !(<NPC>)
  RETURN 0
ENDIF
SERV.B {MARKER}|fn|[<UID>]
SERV.B {MARKER}|memobj|[<FINDLAYER(30)>]|[<FINDLAYER(30).TYPE>]|[<FINDLAYER(30).COLOR>]|[<FINDLAYER(30).LINK.SERIAL>]
SERV.B {MARKER}|memory|[<memoryFindType(MEMORY_ISPAWNED).ISITEM>]
ARG(mySpawn,0)
IF (FLAG_SPAWNED)
  IF (itemExists(<memoryFindType(MEMORY_ISPAWNED).LINK>))
    ARG(mySpawn,<memoryFindType(MEMORY_ISPAWNED).LINK>)
    SERV.B {MARKER}|guard|[passed]
  ELSE
    SERV.B {MARKER}|guard|[failed]
  ENDIF
ENDIF
SERV.B {MARKER}|argserial|[<mySpawn.SERIAL>]
SERV.B {MARKER}|serial|[<mySpawn.SERIAL>]
SERV.B {MARKER}|before|[<mySpawn.P>]
mySpawn.P=<P>
mySpawn.MORE2=17
mySpawn.TAG(probe,17)
IF (mySpawn.MORE2 >= mySpawn.AMOUNT)
  SERV.B {MARKER}|bareif|[passed]
ELSE
  SERV.B {MARKER}|bareif|[failed]
ENDIF
mySpawn.TAG(spawnEventsCount,1)
ARG(i,0)
WHILE (<ARG(i)> < <EVAL mySpawn.TAG(spawnEventsCount)>)
  ARG(i,#+1)
ENDWHILE
mySpawn.creaturestart_nested(17)
SERV.B {MARKER}|after|[<mySpawn.P>]
SERV.B {MARKER}|more2|[<mySpawn.MORE2>]
SERV.B {MARKER}|tag|[<mySpawn.TAG(probe)>]
SERV.B {END_MARKER}
RETURN 1

[FUNCTION creaturestart_nested]
SERV.B {MARKER}|nested|[<UID>]|[<ARGV(0)>]
RETURN 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}|spawn|[<FINDUID({SPAWN_SERIAL}).TYPE>]|[<FINDUID({SPAWN_SERIAL}).MORE1>]|[<FINDUID({SPAWN_SERIAL}).MORE2>]
FINDUID({SPAWN_SERIAL}).TIMER=1
RETURN 0
""",
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
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE="Sphere synthetic bare ARG object-root fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDITEM {SPAWN_DEFNAME}]
SERIAL={SPAWN_SERIAL}
MORE1="{NPC_DEFNAME}"
MORE2=2
AMOUNT=10
MOREP=1,1,1
P=128,128,0
TIMER=1
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic bare ARG object-root fixture"
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

    write_mul_fixture(output, extra_item_id=max(ITEM_ID, DRIVER_ID, 0x2007))
    return 0


MODE = register_mode(
    FixtureMode(
        name="bare-arg-object-root",
        fixture_args=(),
        order=190,
        id_block=136,
        case=FixtureCase(
            name="bare-arg-object-root",
            mode="bare-arg-object-root",
            tests=(TestCase("test_bare_arg_object_root.py", (), True, True),),
            ports={"native": 3174, "asan": 3175},
            output="bare-arg-object-root",
        ),
    )
)
