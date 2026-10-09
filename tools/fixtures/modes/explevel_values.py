"""Synthetic fixture for fixed-point skill text and numeric skill updates."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ExplevelValuesProbe"
PASSWORD = "explevel-pw"
EVENT_NAME = "e_ExplevelValuesProbe"
MARKER = "SPHERE_EXPLEVEL_VALUES"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    """Generate a self-contained character that exercises the level page values."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables_text = tables.read_text(encoding="ascii")
    # Keep the two resource roots generic while exercising the dotted fields
    # used by the level page's mind-power and mind-defence calculations.
    for skill_number, defname, key in (
        (16, "Skill_EvalInt", "EI"),
        (17, "Skill_MagicResist", "MagicResist"),
    ):
        start = tables_text.index(f"[SKILL {skill_number}]\n")
        end = tables_text.index(f"[SKILL {skill_number + 1}]\n", start)
        block = tables_text[start:end]
        block = block.replace(
            f"[SKILL {skill_number}]\nKEY=SYNTH_SKILL_{skill_number}",
            f"[SKILL {skill_number}]\nDEFNAME={defname}\nKEY={key}",
            1,
        ).replace("EFFECT=0", "EFFECT=100", 1)
        tables_text = tables_text[:start] + block + tables_text[end:]

    tables.write_text(
        tables_text
        + f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
TAG(hitspeed,20971)
TAG(SM,301535)
TAG(OM,362395)
SYSMESSAGE {MARKER} C|mind|[<Skill_EvalInt.effect>|<Skill_MagicResist.effect>|<eval ((0*Skill_EvalInt.effect)+(36*700))/1000>|<eval ((0*Skill_MagicResist.effect)+500)/1000>]
SYSMESSAGE {MARKER} C|before|[<MAGERY>|<RESIST>|<eval MAGERY>|<eval RESIST>]
MAGERY=<eval MAGERY+10>
SYSMESSAGE {MARKER} C|after|[<MAGERY>|<RESIST>|<eval MAGERY>|<eval RESIST>]
SYSMESSAGE {MARKER} C|tags|[<tag.hitspeed>|<tag.SM>|<tag.OM>]
SYSMESSAGE {MARKER} C|qval|[<qval(1,"shown",)>|<qval(0,"yes","no")>|<qval(-1,"negative","zero","positive")>]
SYSMESSAGE {MARKER} C|qval_expr|[<qval(42==31,"shown",)>]
SYSMESSAGE {END_MARKER}
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
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
        'TITLE="Sphere synthetic explevel-values fixture"\n'
        'VERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic explevel-values fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ExplevelValuesProbeCharacter
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
MAGERY=300
RESIST=0
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="explevel-values",
        fixture_args=(),
        order=168,
        id_block=113,
        case=FixtureCase(
            name="explevel-values",
            mode="explevel-values",
            tests=(TestCase("test_explevel_values.py", (), True, True),),
            ports={"native": 3154, "asan": 3155},
            mode_by_variant={},
            output="explevel-values",
        ),
    )
)
