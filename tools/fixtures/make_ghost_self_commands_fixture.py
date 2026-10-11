#!/usr/bin/env python3
"""Generate the dead-player self-command fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.compat_writer import write_mul_fixture, write_runtime_files, write_scripts


GM_ACCOUNT = "GhostSelfGm"
GM_PASSWORD = "ghost-gm-pw"
GM_SERIAL = 2
PLAYER_ACCOUNT = "GhostSelfPlayer"
PLAYER_PASSWORD = "ghost-player-pw"
PLAYER_SERIAL = 1


def generate(output: Path) -> int:
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    if any(output.iterdir()):
        raise ValueError(f"output directory is not empty: {output}")

    write_runtime_files(output)
    write_scripts(output)
    write_mul_fixture(output)

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[{GM_ACCOUNT}]
PASSWORD={GM_PASSWORD}
PLEVEL=GM
CHARUID={GM_SERIAL}
LASTCHARUID={GM_SERIAL}
[{PLAYER_ACCOUNT}]
PASSWORD={PLAYER_PASSWORD}
PLEVEL=Player
CHARUID={PLAYER_SERIAL}
LASTCHARUID={PLAYER_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "accounts" / "sphereacct.scp").write_text("[EOF]\n", encoding="ascii")

    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic ghost self-command fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic ghost self-command fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={PLAYER_SERIAL}
ACCOUNT={PLAYER_ACCOUNT}
NAME=GhostSelfPlayerCharacter
EVENTS=e_AllPlayers
STR=100
DEX=100
INT=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDCHAR c_MAN]
SERIAL={GM_SERIAL}
ACCOUNT={GM_ACCOUNT}
NAME=GhostSelfGmCharacter
EVENTS=e_AllPlayers
STR=100
DEX=100
INT=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )

    (output / "scripts" / "spheretables.scp").write_text(
        (output / "scripts" / "spheretables.scp").read_text(encoding="ascii")
        + """

[PLEVEL 1]
WHERE
WHOAMI
EXPLEVEL

[FUNCTION where]
SYSMESSAGE GHOST_SELF_WHERE_OK <P>
RETURN 0

[FUNCTION whoami]
SYSMESSAGE GHOST_SELF_WHOAMI_OK
RETURN 0

[FUNCTION explevel]
DIALOG d_ghost_self_explevel
RETURN 0

[DIALOG d_ghost_self_explevel]
100,100
page 0
resizepic 0 0 2620 250 120
text 20 20 0 0
button 20 60 2445 2445 1 0 1

[DIALOG d_ghost_self_explevel BUTTON]
ON=1
SYSMESSAGE GHOST_SELF_EXPLEVEL_BUTTON
RETURN 0
""",
        encoding="ascii",
    )
    print(f"wrote synthetic Sphere runtime fixture to {output}")
    return 0


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_ghost_self_commands_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
