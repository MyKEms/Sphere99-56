"""Registered synthetic fixture for dynamic ``contents()`` assignments."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "DynContents"
PASSWORD = "dyn-pw"
EVENT_NAME = "e_DynamicContentsProbe"
MARKER = "SPHERE_DYNAMIC_CONTENTS"

SOURCE_SERIAL = 1000
CHILD_SERIAL = 1001
DESTINATION_SERIAL = 1002
ITEM_UID_BASE = 0x40000000
SOURCE_UID = ITEM_UID_BASE | SOURCE_SERIAL
CHILD_UID = ITEM_UID_BASE | CHILD_SERIAL
DESTINATION_UID = ITEM_UID_BASE | DESTINATION_SERIAL
CHAR_SERIAL = 3


def _insert_before_eof(path: Path, text: str) -> None:
    contents = path.read_text(encoding="ascii")
    marker = "[EOF]"
    index = contents.lower().rfind(marker.lower())
    if index < 0:
        path.write_text(contents + text, encoding="ascii")
        return
    path.write_text(contents[:index] + text + contents[index:], encoding="ascii")


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini = output / "sphere.ini"
    ini_text = ini.read_text(encoding="ascii")
    if "UNKNOWNKEYWORDREPORT=" not in ini_text:
        ini.write_text(
            ini_text.replace(
                "DEBUGLEVEL=0\n",
                "DEBUGLEVEL=0\nUNKNOWNKEYWORDREPORT=logs/unknown-keywords.json\n",
                1,
            ),
            encoding="ascii",
        )

    tables = output / "scripts" / "spheretables.scp"
    _insert_before_eof(
        tables,
        f"""

[FUNCTION contents]
if (<safe rescount>)
  arg(count,<rescount>)
  while (arg(count))
    arg(count,<eval <arg(count)>-1>)
    findcont(<arg(count)>).<args>
  endwhile
endif

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER}_BEFORE <FINDUID({CHILD_UID}).CONT.SERIAL>|<FINDUID({SOURCE_UID}).RESCOUNT>|<FINDUID({DESTINATION_UID}).SERIAL>
FINDUID({SOURCE_UID}).CONTENTS(CONT=<FINDUID({DESTINATION_UID})>)
SYSMESSAGE {MARKER}_AFTER <FINDUID({CHILD_UID}).CONT.SERIAL>|<FINDUID({CHILD_UID}).TOPOBJ.SERIAL>|<FINDUID({DESTINATION_UID}).SERIAL>
SYSMESSAGE {MARKER}_END
RETURN 0
""",
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
    header = "TITLE=Sphere synthetic dynamic contents fixture\nVERSION=0.99\nSAVECOUNT=0\n"
    (output / "save" / "sphereworld.scp").write_text(
        header
        + f"""[WORLDITEM DEFAULTITEM]
SERIAL={SOURCE_SERIAL}
P=128,128,0
[WORLDITEM SYNTHETIC_OBJECT]
SERIAL={CHILD_SERIAL}
CONT={SOURCE_UID}
AMOUNT=1
[WORLDITEM DEFAULTITEM]
SERIAL={DESTINATION_SERIAL}
P=130,128,0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        header
        + f"""[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
NAME=DynamicContentsProbe
ACCOUNT={ACCOUNT}
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

    from .compat_writer import write_mul_fixture

    write_mul_fixture(output)
    return 0


MODE = register_mode(
    FixtureMode(
        name="dynamic-contents",
        fixture_args=(),
        order=159,
        id_block=105,
        case=FixtureCase(
            name="dynamic-contents",
            mode="dynamic-contents",
            tests=(TestCase("test_dynamic_contents.py", (), True, True),),
            ports={"native": 3150, "asan": 3151},
            output="dynamic-contents",
        ),
    )
)
