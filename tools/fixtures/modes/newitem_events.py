"""Registered fixture for HVAL-backed NEWITEM and tag-derived EVENTS."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NewItemEventsProbe"
PASSWORD = "newitempw"
EVENT_NAME = "e_NewItemEventsProbe"
ITEM_ID = 0xA5E0
FULL_ITEM_ID = f"0a200{ITEM_ID:04x}"
ITEM_NAME = "SYNTHETIC_NEWITEM_EVENTS"
MARKER = "SPHERE_NEWITEM_EVENTS"


def generate(output: Path) -> int:
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

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic newitem events
TYPE=T_NORMAL

[EVENTS {EVENT_NAME}]
ON=@LogIn
IF <TAG.probe_done>
RETURN 0
ENDIF
TAG.probe_done=1
SYSMESSAGE {MARKER} NEWITEM_ZERO|[ready]
NEWITEM(<HVAL 00>)
SYSMESSAGE {MARKER} EVENTS_EMPTY|[ready]
EVENTS
ARG(item_id,0A5E0)
NEWITEM {ITEM_NAME}
SYSMESSAGE {MARKER} DIRECT|[<LASTNEW.NAME>]
LASTNEW.REMOVE
NEWITEM(<HVAL <ARG(item_id)>>)
SYSMESSAGE {MARKER} HVAL_ARG|[<LASTNEW.NAME>]
LASTNEW.REMOVE
NEWITEM(<HVAL 0A5E0>)
SYSMESSAGE {MARKER} HVAL_LITERAL|[<LASTNEW.NAME>]
LASTNEW.REMOVE
NEWITEM(<HVAL {FULL_ITEM_ID}>)
SYSMESSAGE {MARKER} HVAL_FULL_RESOURCE|[<LASTNEW.NAME>]
LASTNEW.REMOVE
f_newitem_events_args({ITEM_NAME})
SRC.f_newitem_events_src_args({ITEM_NAME})
f_newitem_events_argv({ITEM_NAME})
SRC.f_newitem_events_safe({ITEM_NAME})
TAG(spawnEvent,{EVENT_NAME})
EVENTS <TAG(spawnEvent)>
SYSMESSAGE {MARKER} EVENTS_PLAIN|[<ISEVENT({EVENT_NAME})>]
EVENTS +<TAG(spawnEvent)>
SYSMESSAGE {MARKER} EVENTS_PLUS|[<ISEVENT({EVENT_NAME})>]
EVENTS -<TAG(spawnEvent)>
SYSMESSAGE {MARKER} EVENTS_MINUS|[<ISEVENT({EVENT_NAME})>]
EVENTS "<TAG(spawnEvent)>"
SYSMESSAGE {MARKER} EVENTS_QUOTED|[<ISEVENT({EVENT_NAME})>]
ARG(event_index,0)
TAG(spawnEvent[0],{EVENT_NAME})
EVENTS <TAG(spawnEvent[<ARG(event_index)>])>
SYSMESSAGE {MARKER} EVENTS_INDEXED_TAG|[<ISEVENT({EVENT_NAME})>]
SYSMESSAGE {MARKER}_END
RETURN 0

[FUNCTION f_newitem_events_args]
NEWITEM(<HVAL ARGS>)
SYSMESSAGE {MARKER} HVAL_ARGS|[<LASTNEW.NAME>]
LASTNEW.REMOVE
RETURN 0

[FUNCTION f_newitem_events_src_args]
NEWITEM(<HVAL ARGS>)
SYSMESSAGE {MARKER} HVAL_SRC_ARGS|[<LASTNEW.NAME>]
LASTNEW.REMOVE
RETURN 0

[FUNCTION f_newitem_events_argv]
NEWITEM(<HVAL ARGV(0)>)
SYSMESSAGE {MARKER} HVAL_ARGV|[<LASTNEW.NAME>]
LASTNEW.REMOVE
RETURN 0

[FUNCTION f_newitem_events_safe]
IF (STRLEN("<ARGS>"))
IF (ISITEM)
SYSMESSAGE {MARKER} HVAL_SAFE_ITEM|[rejected]
RETURN 0
ENDIF
ARG(saved_act,<ACT>)
NEWITEM(<HVAL ARGS>)
SYSMESSAGE {MARKER} HVAL_SAFE_ARGS|[<LASTNEW.NAME>]
LASTNEW.REMOVE
ACT=<ARG(saved_act)>
ENDIF
RETURN 0
""",
        encoding="ascii",
    )

    accounts = output / "accounts" / "sphereaccu.scp"
    accounts.write_text(
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
        'TITLE="Sphere synthetic NEWITEM/EVENTS fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic NEWITEM/EVENTS fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=NewItemEventsProbeCharacter
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

    from .compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="newitem-events",
        fixture_args=(),
        order=194,
        id_block=146,
        case=FixtureCase(
            name="newitem-events",
            mode="newitem-events",
            tests=(TestCase("test_newitem_events.py", (), True, True),),
            ports={"native": 3210, "asan": 9210},
            output="newitem-events",
        ),
    )
)
