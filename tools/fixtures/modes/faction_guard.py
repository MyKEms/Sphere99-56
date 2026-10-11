"""Registered fixture for faction-aware guard reactions."""

from __future__ import annotations

from pathlib import Path

from .compat_writer import write_mul_fixture
from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


SAME_ACCOUNT = "FGSame"
SAME_PASSWORD = "faction-same-pw"
SAME_SERIAL = 3
OUTCAST_ACCOUNT = "FGOutcast"
OUTCAST_PASSWORD = "faction-out-pw"
OUTCAST_SERIAL = 4
SAME_GUARD_SERIAL = 5
OUTCAST_GUARD_SERIAL = 6
SAME_POINT = "127,128,0"
SAME_GUARD_POINT = "128,128,0"
OUTCAST_POINT = "199,128,0"
OUTCAST_GUARD_POINT = "200,128,0"
SAME_MARKER = "FACTION_GUARD_SAME"
OUTCAST_MARKER = "FACTION_GUARD_OUTCAST"


def _account_file() -> str:
    return f"""[ACCOUNT {SAME_ACCOUNT}]
PASSWORD={SAME_PASSWORD}
CHARUID={SAME_SERIAL}
LASTCHARUID={SAME_SERIAL}
[ACCOUNT {OUTCAST_ACCOUNT}]
PASSWORD={OUTCAST_PASSWORD}
CHARUID={OUTCAST_SERIAL}
LASTCHARUID={OUTCAST_SERIAL}
[EOF]
"""


def _player(serial: int, account: str, name: str, point: str, realm: int) -> str:
    return f"""[WORLDCHAR c_MAN]
SERIAL={serial}
ACCOUNT={account}
NAME={name}
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
KARMA=10000
TAG.realm={realm}
P={point}
"""


def _guard(serial: int, point: str) -> str:
    return f"""[WORLDCHAR SYNTHETIC_FACTION_GUARD]
SERIAL={serial}
NAME=Faction guard probe
NPC=brain_human
STR=1000
INT=100
DEX=300
HITS=1000
MAXHITS=1000
MANA=100
STAM=300
TAG.guard=1
P={point}
"""


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[CHARDEF SYNTHETIC_FACTION_GUARD]
DEFNAME=SYNTHETIC_FACTION_GUARD
ID=c_man
NAME=synthetic faction guard
TAG.guard=1
TEVENTS=e_SyntheticFactionGuard
ON=@Create
NPC=brain_human

[FUNCTION ismyguard]
IF (<NPC>)
  IF (<TYPEDEF.TAG.GUARD>)
    IF (<SRC.ISPLAYER>)
      IF (<TYPEDEF.TAG.GUARD> == 1)
        IF (<SRC.TAG.REALM> > 0)
          IF (!(<SRC.FLAG_CRIMINAL>))
            IF (!(<SRC.ISMURDERER>))
              IF (<SRC.TAG.REALM> == 1)
                RETURN 1
              ENDIF
            ENDIF
          ENDIF
        ENDIF
      ENDIF
    ENDIF
  ENDIF
ENDIF
RETURN 0

[EVENTS e_SyntheticFactionGuard]
ON=@NPCSeeNewPlayer
IF (ismyguard)
SAY {SAME_MARKER}
RETURN 1
ENDIF
SAY {OUTCAST_MARKER}
ATTACK <SRC>
RETURN 1
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        _account_file(), encoding="ascii"
    )
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic faction guard fixture\nVERSION=0.99\nSAVECOUNT=0\n[EOF]\n",
        encoding="ascii",
    )
    chars = """TITLE=Sphere synthetic faction guard fixture
VERSION=0.99
SAVECOUNT=0
"""
    chars += _player(SAME_SERIAL, SAME_ACCOUNT, "FactionSame", SAME_POINT, 1)
    chars += _player(OUTCAST_SERIAL, OUTCAST_ACCOUNT, "FactionOutcast", OUTCAST_POINT, 0)
    chars += _guard(SAME_GUARD_SERIAL, SAME_GUARD_POINT)
    chars += _guard(OUTCAST_GUARD_SERIAL, OUTCAST_GUARD_POINT)
    chars += "[EOF]\n"
    (output / "save" / "spherechars.scp").write_text(chars, encoding="ascii")
    write_mul_fixture(output)
    return 0


MODE = register_mode(
    FixtureMode(
        name="faction-guard",
        fixture_args=(),
        order=188,
        id_block=134,
        case=FixtureCase(
            name="faction-guard",
            mode="faction-guard",
            tests=(TestCase("test_faction_guard.py", (), True, True),),
            ports={"native": 3200, "asan": 3201},
            output="faction-guard",
        ),
    )
)
