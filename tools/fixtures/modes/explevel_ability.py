"""Synthetic fixture for the indexed ability-definition path used by .explevel."""

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "ExplevelAbility"
PASSWORD = "exab-pw"
EVENT_NAME = "e_ExplevelAbilityProbe"
MARKER = "SPHERE_EXPLEVEL_ABILITY"
END_MARKER = MARKER + "_END"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + f"""

[defnames def_rearAbilities]
def_rearAbilities_number     1
def_rearAbilities_1          manareg
def_rearAbilities_manareg_desc "Regenerace many"
def_rearAbilities_manareg[0] 0,20
def_rearAbilities_manareg_class[0] // everyone

[FUNCTION f_ra_add_hasClass]
arg(i,0)
arg(abilityRestrictions,<safe.def_rearAbilities_<args>[<eval i>]>)
while (strlen(<arg(abilityRestrictions)>))
  if (f_ra_add_hasClassSingle(<i>,<profession>,<safe.def_rearAbilities_<args>_class[<eval i>]>))
    return <i>
  endif
  arg(i,#+1)
  arg(abilityRestrictions,<safe.def_rearAbilities_<args>[<eval i>]>)
endwhile
return -1

[FUNCTION f_ra_add_hasClassSingle]
if (<argv(0)> == 0) && (strlen(<argv(2)>) == 0)
  return 1
endif
arg(i,2)
while (i < argvCount)
  if (!strcmpi("<argv(1)>","class_<argv(<i>)>"))
    return 1
  endif
  arg(i,#+1)
endwhile
return 0

[FUNCTION f_ra_add]
tag(ra_<args>,<?eval tag(ra_<args>)?>+1)
return 0

[EVENTS {EVENT_NAME}]
ON=@LogIn
SYSMESSAGE {MARKER} C|before|[<eval tag(ra_manareg)>]
f_ra_add(manareg)
SYSMESSAGE {MARKER} C|after|[<eval tag(ra_manareg)>]
SYSMESSAGE {MARKER} C|indexed|[<safe.def_rearAbilities_manareg[0]>]
SYSMESSAGE {MARKER} C|name|[<def_rearAbilities[1]>]
SYSMESSAGE {MARKER} C|function|[<f_ra_add_hasClass(manareg)>]
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
        'TITLE="Sphere synthetic explevel ability fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n[EOF]\n',
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE="Sphere synthetic explevel ability fixture"
VERSION="0.99z8"
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL=3
NAME=ExplevelAbilityProbeCharacter
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
P=128,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="explevel-ability",
        fixture_args=(),
        order=179,
        id_block=126,
        case=FixtureCase(
            name="explevel-ability",
            mode="explevel-ability",
            tests=(TestCase("test_explevel_ability.py", (), True, True),),
            ports={"native": 4610, "asan": 4611},
            mode_by_variant={},
            output="explevel-ability",
        ),
    )
)
