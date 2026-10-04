#!/usr/bin/env python3
"""Generate the synthetic stat-update dialog fixture."""

from __future__ import annotations

import argparse
from pathlib import Path
import subprocess
import sys


ACCOUNT = "StatusDialogProbe"
LOGIN_VALUE = "status-dialog-probe-pw"
MARKER = "SPHERE_STATUS_DIALOG"
BUTTON_ID = 7
TEXT_LABEL = "Update stats"
ITEM_NAME = "SYNTHETIC_STATUS_DIALOG"
ITEM_ID = 0x0E78
ITEM_UID = 0x40000020
ITEM_POINT = (129, 128, 0)


DIALOG_SECTIONS = f"""[DIALOG 0]
0 0
resizepic 0 0 5054 240 120
button 20 20 4005 4006 1 0 {BUTTON_ID}
text 60 20 0 0

[DIALOG 0 TEXT]
{TEXT_LABEL}

[DIALOG 0 BUTTON]
ON={BUTTON_ID}
SRC.STR=123
SRC.DEX=77
SRC.INT=88
SRC.SYSMESSAGE {MARKER}|{BUTTON_ID}|<SRC.STR>|<SRC.DEX>|<SRC.INT>
"""

# RES_Dialog is the eleventh resource type (RES_UNKNOWN is zero), and the
# first numbered dialog occupies index zero.
DIALOG_RID = 0x96000000


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    output = args.output.resolve()
    if output.exists() and any(output.iterdir()):
        parser.error(f"output directory is not empty: {output}")
    output.mkdir(parents=True, exist_ok=True)

    tools_dir = Path(__file__).resolve().parent
    subprocess.run(
        [
            sys.executable,
            str(tools_dir / "make_fixture.py"),
            str(output),
            "--roundtrip-integrity-probe",
        ],
        check=True,
    )

    script_path = output / "scripts" / "spheretables.scp"
    script = script_path.read_text(encoding="ascii")
    script += f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic status dialog trigger
TYPE=T_NORMAL
ON=@UserDClick
DIALOG 0x{DIALOG_RID:08X}
RETURN 1
"""
    script_path.write_text(DIALOG_SECTIONS + "\n" + script, encoding="ascii")

    # The trigger item is a public synthetic definition, so add its tile to
    # the generated MUL fixture instead of depending on a client install.
    from modes.compat_writer import write_mul_fixture

    write_mul_fixture(output, extra_item_id=ITEM_ID)

    x, y, z = ITEM_POINT
    (output / "save" / "sphereworld.scp").write_text(
        f"""TITLE=Sphere synthetic status dialog fixture
VERSION=0.99
SAVECOUNT=0
[WORLDITEM {ITEM_NAME}]
SERIAL=0{ITEM_UID:x}
P={x},{y},{z}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        """TITLE=Sphere synthetic status dialog fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    print(f"wrote synthetic status-dialog runtime fixture to {output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
