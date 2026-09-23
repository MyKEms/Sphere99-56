#!/usr/bin/env python3
"""Generate a small gump-and-skill runtime from the shared fixture writer.

The fixture opens a numbered gump for one synthetic account.  Button 7 sets a
server-side tag and emits a marker; the test client locates the button by its
text label, sends a 0xB1 reply, and verifies the marker.  A numeric dialog id
keeps this probe independent of the engine's named-dialog compatibility fix.
"""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


ACCOUNT = "GumpSkillProbe"
LOGIN_VALUE = "gump-skill-probe-pw"
MARKER = "SPHERE_GUMP_SKILL_CLICKED"
BUTTON_ID = 7
TEXT_LABEL = "Choose class"


DIALOG_SECTIONS = f"""[DIALOG 0]
0 0
resizepic 0 0 5054 240 120
button 20 20 4005 4006 1 0 {BUTTON_ID}
text 60 20 0 0

[DIALOG 0 TEXT]
{TEXT_LABEL}

[DIALOG 0 BUTTON]
ON={BUTTON_ID}
TAG.gump_skill_clicked=1
SYSMESSAGE {MARKER}|{BUTTON_ID}|<TAG.gump_skill_clicked>
"""

# RES_Dialog is the tenth resource type (RES_UNKNOWN is zero), and the first
# numbered dialog occupies index zero.  Use the complete RID so this fixture
# remains usable before named-dialog lookup is available.
DIALOG_RID = 0x94000000


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error(f"output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    tools_dir = Path(__file__).resolve().parent
    make_fixture = tools_dir / "make_fixture.py"
    subprocess.run(
        [
            sys.executable,
            str(make_fixture),
            str(output),
            "--roundtrip-integrity-probe",
        ],
        check=True,
    )

    script_path = output / "scripts" / "spheretables.scp"
    script = script_path.read_text(encoding="ascii")
    login_marker = "[EVENTS e_AllPlayers]\nON=@LogIn\n"
    if script.count(login_marker) != 1:
        raise RuntimeError("synthetic login event was not found exactly once")
    login = login_marker + f"DIALOG 0x{DIALOG_RID:08X}\n"
    script = script.replace(login_marker, login, 1)
    script_path.write_text(DIALOG_SECTIONS + "\n" + script, encoding="ascii")
    print(f"wrote synthetic gump/skill runtime fixture to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
