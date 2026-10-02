"""Registered fixture for stock's space-separated ``SEX`` escape form."""

from __future__ import annotations

from pathlib import Path

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "SexFormProbe"
PASSWORD = "sex_form_pw"
MALE_SERIAL = 3
FEMALE_SERIAL = 4
MARKER = "SPHERE_SEX"


def generate(output: Path) -> int:
    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    ini = output / "sphere.ini"
    ini.write_text(
        ini.read_text(encoding="ascii").replace("CLIENTLINGER=60", "CLIENTLINGER=0"),
        encoding="ascii",
    )

    scripts = output / "scripts" / "spheretables.scp"
    script_text = scripts.read_text(encoding="ascii")
    script_text = script_text.replace(
        "DEFNAME=c_WOMAN\nNAME=synthetic human\n",
        "DEFNAME=c_WOMAN\nNAME=synthetic human\nCAN=0x0800\n",
        1,
    )
    scripts.write_text(
        script_text
        + f"""

[EVENTS e_synthetic_sex_form]
ON=@LogIn
SYSMESSAGE {MARKER} space=[<SEX Male Female>]
SYSMESSAGE {MARKER} comma=[<SEX(Male,Female)>]
RACEMESSAGE("<SEX Ziskal Ziskala> jsi 20 zkusenosti.")
RETURN 0

[FUNCTION f_strtoascii]
VAR(asciitext,"")
ARG(u,0)
ARG(asciilen,<EVAL <ARGV(0)>>)
WHILE (<ARG(u)> < <ARG(asciilen)>)
  VAR(asciitext,"<?SAFE asciitext?> <?HVAL STRGETASCII(\"<ARGV(1)>\",<ARG(u)>)?>")
  ARG(u,<EVAL <ARG(u)>+1>)
ENDWHILE
RETURN <asciitext>

[FUNCTION f_sysmessagecol]
IF (<ARGVCOUNT> != 2)
  RETURN 0
ENDIF
ARG(length,<STRLEN(<ARGV(1)>)>+45)
IF (<ARG(length)> > <EVAL (83+45)>)
  ARG(length,<EVAL (83+45)>)
ENDIF
VAR(packet,01c <?HVAL (<ARG(length)>&0ff00)/0100?> <?HVAL <ARG(length)>&0ff?> 00 00 00 00 00 00 02 <?HVAL (<ARGV(0)>&0ff00)/0100?> <?HVAL <ARGV(0)>&0ff?> 00 03 053 079 073 074 065 06d 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 00 <?F_STRTOASCII((<ARG(length)>-45),\"<ARGV(1)>\")?> 00)
SENDPACKET(<VAR(packet)>)
RETURN 0

[FUNCTION racemessage]
f_sysmessagecol(057,"<ARGS>")
RETURN 0
""",
        encoding="ascii",
    )

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
CHARUID={MALE_SERIAL}
LASTCHARUID={MALE_SERIAL}
CHARUID={FEMALE_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic SEX fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic SEX fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={MALE_SERIAL}
ACCOUNT={ACCOUNT}
NAME=SexFormMale
EVENTS=e_synthetic_sex_form
STR=100
DEX=100
INT=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
[WORLDCHAR c_WOMAN]
SERIAL={FEMALE_SERIAL}
ACCOUNT={ACCOUNT}
NAME=SexFormFemale
EVENTS=e_synthetic_sex_form
STR=100
DEX=100
INT=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=130,128,0
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="sex-form",
        fixture_args=(),
        # Blocks 91 and 92 are assigned to other fixture modes.
        order=93,
        id_block=93,
        case=FixtureCase(
            name="sex-form",
            mode="sex-form",
            tests=(TestCase("test_sex_form.py", (), True, True),),
            ports={"native": 2972, "asan": 2974},
            output="sex-form",
        ),
    )
)
