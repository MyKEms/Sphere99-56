"""Registered synthetic fixture for named NPC brain creation and dispatch."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "NpcBrainProbe"
LOGIN_TOKEN = "npc-brain-pw"
EVENT_NAME = "e_NpcBrainProbe"
MARKER = "SPHERE_NPC_BRAIN"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[DEFNAMES BRAINS]
BRAIN_ANIMAL 1

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} player-npc=<NPC>
NEWNPC c_NpcBrainProbe
LASTNEW.P=129,128,0
SYSMESSAGE {MARKER} created
NEWNPC c_NpcBrainDefault
LASTNEW.P=130,128,0
SYSMESSAGE {MARKER} default-created
NEWNPC c_NpcBrainAlias
LASTNEW.P=131,128,0
SYSMESSAGE {MARKER} alias-created
RETURN 0

[CHARDEF c_NpcBrainProbe]
DEFNAME=c_NpcBrainProbe
ID=c_man
NAME=synthetic npc brain probe
ON=@Create
NPC=brain_animal
RETURN 0

[CHARDEF c_NpcBrainDefault]
DEFNAME=c_NpcBrainDefault
ID=c_man
NAME=synthetic npc brain default probe
ON=@Create
NPC=brain_missing
RETURN 0

[CHARDEF c_NpcBrainAlias]
DEFNAME=c_NpcBrainAlias
ID=c_man
NAME=synthetic npc brain alias probe
ON=@Create
NPC=brain_berserk
RETURN 0

""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={LOGIN_TOKEN}
LASTCHARUID=3
CHARUID=3
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic NPC dispatch fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic NPC dispatch fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=NpcBrainProbe
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
NPC=0
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
        name="npc-brain",
        fixture_args=(),
        order=95,
        id_block=95,
        case=FixtureCase(
            name="npc-brain",
            mode="npc-brain",
            tests=(TestCase("test_npc_brain.py", (), True, True),),
            ports={"native": 4594, "asan": 4595},
            output="npc-brain",
        ),
    )
)
