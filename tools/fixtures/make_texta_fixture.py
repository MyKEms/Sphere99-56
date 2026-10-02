#!/usr/bin/env python3
"""Create a small runtime containing both legacy TEXTA spellings."""

from __future__ import annotations

from pathlib import Path

from modes.base import MODE as BASE_MODE
from modes.compat_writer import write_mul_fixture
from modes.legacy_generator import generate as generate_recipe


ACCOUNT = "TextaProbe"
PASSWORD = "texta-pw"
CHAR_SERIAL = 3
ITEM_ID = 0x0EAF
ITEM_NAME = "SYNTHETIC_TEXTA_STONE"
ITEM_SERIAL = 0x40000030


def _scripts() -> str:
    return f"""

[ITEMDEF 0x{ITEM_ID:04X}]
DEFNAME={ITEM_NAME}
NAME=synthetic inline text stone
TYPE=T_NORMAL
ON=@UserDClick
DIALOG d_texta_probe
RETURN 1

[DIALOG d_texta_probe]
0 0
gumppic 140 200 2200
texta 180 233 1000 \"(80 - 120)\"
argo.texta(180,269,1000,\"(60 - 90)\")
texta 180 305 1000 \"(20 - 50)\"
argo.texta(180,341,1000,\"(80 - 130)\")
"""


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    (output / "scripts" / "spheretables.scp").write_text(
        (output / "scripts" / "spheretables.scp").read_text(encoding="ascii")
        + _scripts(),
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        "\n".join(
            (
                f"[ACCOUNT {ACCOUNT}]",
                f"PASSWORD={PASSWORD}",
                f"LASTCHARUID={CHAR_SERIAL}",
                f"CHARUID={CHAR_SERIAL}",
                "[EOF]",
            )
        ),
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        "\n".join(
            (
                "TITLE=Sphere synthetic TEXTA fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                f"[WORLDITEM {ITEM_NAME}]",
                f"SERIAL=0{ITEM_SERIAL:x}",
                "P=129,128,0",
                "[EOF]",
            )
        ),
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        "\n".join(
            (
                "TITLE=Sphere synthetic TEXTA fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={CHAR_SERIAL}",
                f"ACCOUNT={ACCOUNT}",
                "NAME=TextaProbeCharacter",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            )
        ),
        encoding="ascii",
    )
    write_mul_fixture(output, extra_item_id=ITEM_ID)
    return 0


if __name__ == "__main__":
    import sys

    raise SystemExit(generate(Path(sys.argv[1])))
