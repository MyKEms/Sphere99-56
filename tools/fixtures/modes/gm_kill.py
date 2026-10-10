"""Registered fixture for targeted GM ``KILL`` commands."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import write_mul_fixture
from .gm_command_log import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "GmKillProbe"
PASSWORD = "gm-kill-pw"
CHAR_NAME = "GmKillCharacter"
NPC_DEFNAME = "SYNTHETIC_GM_KILL_NPC"
TARGET_SERIALS = (4, 5)


def generate(output: Path) -> int:
    """Reuse the authenticated GM runtime and add two deterministic animals."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    account_file = output / "accounts" / "sphereaccu.scp"
    account_file.write_text(
        account_file.read_text(encoding="ascii")
        .replace("GmCommandLogProbe", ACCOUNT)
        .replace("gm_cmd_log_pw", PASSWORD),
        encoding="ascii",
    )

    chars = output / "save" / "spherechars.scp"
    char_text = chars.read_text(encoding="ascii")
    char_text = char_text.replace("GmCommandLogProbe", ACCOUNT).replace(
        "GmCommandLogCharacter", CHAR_NAME
    )
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[TYPEDEF 74]
DEFNAME=T_EQ_MEMORY_OBJ

[ITEMDEF 0x2007]
DEFNAME=i_memory
TYPE=T_EQ_MEMORY_OBJ
LAYER=30

[CHARDEF {NPC_DEFNAME}]
DEFNAME={NPC_DEFNAME}
NAME=synthetic GM kill animal
ID=0x0190
TEVENTS=e_synthetic_gm_kill
STR=20
DEX=20
INT=10
HITS=20
MAXHITS=20
MANA=10
STAM=10

[EVENTS e_synthetic_gm_kill]
ON=@Death
ARG(memory,<MEMORYFINDTYPE(0209c)>)
SAY GM_KILL_DEATH_LINK <ISUIDVALID <ARG(memory).LINK>>
SAY GM_KILL_ACT_REF <SRC.ACT.f_probe_source_act>
SAY GM_KILL_DEATH

[FUNCTION f_probe_source_act]
ACT=0
ACT=<SRC.ACT>
SAY GM_KILL_RESTORED_ACT <ISUIDVALID <ACT>>
RETURN 1
""",
        encoding="ascii",
    )
    insert = "\n".join(
        [
            f"[WORLDCHAR {NPC_DEFNAME}]",
            f"SERIAL={TARGET_SERIALS[0]}",
            "NAME=GM Kill Animal One",
            "NPC=2",
            "STR=100",
            "DEX=100",
            "INT=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            "P=130,128,0",
            "",
            f"[WORLDCHAR {NPC_DEFNAME}]",
            f"SERIAL={TARGET_SERIALS[1]}",
            "NAME=GM Kill Animal Two",
            "NPC=2",
            "STR=100",
            "DEX=100",
            "INT=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            "P=130,129,0",
            "",
            "[WORLDITEM i_memory]",
            "SERIAL=6",
            "COLOR=0200",
            "LINK=3",
            "LAYER=30",
            "CONT=4",
            "",
            "[WORLDITEM i_memory]",
            "SERIAL=7",
            "COLOR=0200",
            "LINK=3",
            "LAYER=30",
            "CONT=5",
        ]
    )
    chars.write_text(char_text.replace("[EOF]", insert + "\n[EOF]"), encoding="ascii")
    write_mul_fixture(output, extra_item_id=0x2007)

    return 0


MODE = register_mode(
    FixtureMode(
        name="gm-kill",
        fixture_args=None,
        order=88,
        id_block=88,
        case=FixtureCase(
            name="gm-kill",
            mode=None,
            tests=(TestCase("test_gm_kill.py", (), True, True),),
            ports={"native": 2950, "asan": 2951},
            generator="make_gm_kill_fixture.py",
            output="gm-kill",
        ),
    )
)
