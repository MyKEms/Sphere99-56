"""Synthetic fixture for dynamic script skill spending."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "SkillSpendProbe"
PASSWORD = "spend-pw"
EVENT_NAME = "e_SkillSpendProbe"
MARKER = "SPHERE_SKILL_SPEND"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables_text = tables.read_text(encoding="ascii")
    # The legacy generator deliberately uses synthetic skill names.  Replace
    # one definition with the real Anatomy key so the probe exercises the
    # dynamic skill lookup used by the level-up scripts.
    tables_text = tables_text.replace(
        "[SKILL 1]\nKEY=SYNTH_SKILL_1",
        "[SKILL 1]\nDEFNAME=SKILL_ANATOMY\nKEY=Anatomy",
        1,
    )
    tables_text = tables_text.replace(
        "[PROFESSION 11]\nDEFNAME=class_fixture",
        "[PROFESSION 11]\nDEFNAME=class_fixture\nAnatomy=100.0",
        1,
    )
    tables.write_text(
        tables_text
        + f"""

[EVENTS {EVENT_NAME}]
ON=@LogIn
ANATOMY=0.0
F_SKILL_SPEND_PROBE
RETURN 0

[DEFNAMES SKILL_SPEND_PROBE]
combskill_Anatomy=1

[ITEMDEF 0x0E7A]
DEFNAME=i_dovedbod
TYPE=T_NORMAL

[FUNCTION F_ROUNDSKILL]
ARG(skillvalue,<eval <arg(skillname)>>)
<arg(skillname)>=<eval ((<arg(skillvalue)>+25)/50)*50>

[FUNCTION F_BODUNASKILL]
RETURN 1

[FUNCTION F_SKILL_SPEND_PROBE]
ARG(skillname,<findres(skill,1).name>)
ARG(skillvalue,<eval <arg(skillname)>>)
F_ROUNDSKILL
SYSMESSAGE {MARKER}|before|[<arg(skillname)>|<eval arg(skillvalue)>|<ANATOMY>|<eval ANATOMY>]
if (<safe combskill_<arg(skillname)>>)
  ARG(skillmax,<?<profession>.<arg(skillname)>?>)
  ARG(skillvalue,<eval <arg(skillname)>>)
  SYSMESSAGE {MARKER}|max|[<eval arg(skillmax)>|<eval rescount(i_dovedbod)>]
  if (<eval arg(skillvalue)+50>>arg(skillmax))
    SYSMESSAGE {MARKER}|blocked|max
  elseif (arg(skillvalue)==250)||(arg(skillvalue)==500)||(arg(skillvalue)==750)
    SYSMESSAGE {MARKER}|blocked|milestone
  else
    if (rescount(i_dovedbod)<F_BODUNASKILL(<arg(skillvalue)>))
      SYSMESSAGE {MARKER}|blocked|points
    else
      <arg(skillname)>=<eval arg(skillvalue)+50>
    endif
  endif
endif
SYSMESSAGE {MARKER}|after|[<arg(skillname)>|<ANATOMY>|<eval ANATOMY>]
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
        f'''TITLE="Sphere synthetic skill-spend fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WorldItem i_dovedbod]
SERIAL=4
CONT=5
AMOUNT=1
[WorldItem DEFAULTITEM]
SERIAL=5
LAYER=21
CONT=3
[EOF]
''',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic skill-spend fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=SkillSpendProbeCharacter
ACCOUNT={ACCOUNT}
EVENTS={EVENT_NAME}
PROFESSION=class_fixture
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
MAXMANA=100
STAM=100
MAXSTAM=100
ANATOMY=0
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="skill-spend",
        fixture_args=(),
        order=170,
        id_block=117,
        case=FixtureCase(
            name="skill-spend",
            mode="skill-spend",
            tests=(TestCase("test_skill_spend.py", (), True, True),),
            ports={"native": 4598, "asan": 4599},
            output="skill-spend",
        ),
    )
)
