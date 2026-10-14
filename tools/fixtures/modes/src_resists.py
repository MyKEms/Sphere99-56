"""Registered fixture for a resistance helper called without a trigger source."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode
from .compat_writer import write_mul_fixture


ACCOUNT = "SourceResistsProbe"
PASSWORD = "srcrespw"
ITEM_ID = 0x0E7A
ITEM_NAME = "SYNTHETIC_SOURCE_RESISTS"
CHAR_SERIAL = 3
ITEM_SERIAL = 4
MARKER = "SPHERE_SOURCE_RESISTS"
END_MARKER = MARKER + "_END"


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
NAME=synthetic source resistance probe
TYPE=T_NORMAL
TIMER=15
ON=@Timer
ARG(result,<f_source_resists_probe>)
FINDUID({CHAR_SERIAL}).SYSMESSAGE {MARKER}|[<ARG(result)>]
FINDUID({CHAR_SERIAL}).SYSMESSAGE {MARKER}_ISSUMMON|[<SRC.ISSUMMON>]
FINDUID({CHAR_SERIAL}).SYSMESSAGE {MARKER}_TAG_TEXT|[<SRC.TAG(name)>]
FINDUID({CHAR_SERIAL}).SYSMESSAGE {MARKER}_TAG_LENGTH|[<STRLEN(<SRC.TAG(name)>)>]
FINDUID({CHAR_SERIAL}).SYSMESSAGE {MARKER}_NAME|[<SRC.NAME>]
FINDUID({CHAR_SERIAL}).SYSMESSAGE {END_MARKER}
RETURN 0

[FUNCTION f_source_resists_probe]
RETURN <SRC.ISSUMMON>
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID={CHAR_SERIAL}
LASTCHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=\"Sphere synthetic source resistance fixture\"
VERSION=\"0.99z8\"
SAVECOUNT=0
[WorldItem {ITEM_NAME}]
SERIAL={ITEM_SERIAL}
P=128,128,0
TIMER=15
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=\"Sphere synthetic source resistance fixture\"
VERSION=\"0.99z8\"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
NAME=SourceResistsProbeCharacter
ACCOUNT={ACCOUNT}
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
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


MODE = register_mode(
    FixtureMode(
        name="src-resists",
        fixture_args=(),
        order=203,
        id_block=153,
        case=FixtureCase(
            name="src-resists",
            mode="src-resists",
            tests=(TestCase("test_src_resists.py", (), True, True),),
            ports={"native": 43211, "asan": 43212},
            output="src-resists",
        ),
    )
)
