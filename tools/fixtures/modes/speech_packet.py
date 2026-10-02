"""Registered synthetic fixture mode for NPC speech packet framing."""

from pathlib import Path
import re

from .base import MODE as BASE_MODE
from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "SpeechPacketProbe"
PASSWORD = "speech-pw"
CHAR_SERIAL = 3
NPC_DEFNAME = "c_SYNTHETIC_SPEAKER"
REAL_NOTICE = "Tutorial progress recorded"
REAL_SPEECH_OPTIONS = (
    "Take the north path, <SRC.NAME>, and ask the town guides for work.",
    "The town is safe today, <SRC.NAME>; walk east to meet the guides.",
    "This island trains new adventurers like <SRC.NAME>; keep exploring.",
)


def _system_packet_tokens(text: str) -> str:
    payload = text.encode("ascii") + b"\0"
    # The fixed speech header is 44 bytes; the one-byte flexible-array member
    # accounts for the terminating byte in the wire length.
    length = 44 + len(payload)
    packet = bytearray(
        [
            0x1C,
            (length >> 8) & 0xFF,
            length & 0xFF,
            0,
            0,
            0,
            0,
            0,
            0,
            2,
            0,
            0x39,
            0,
            3,
        ]
    )
    packet.extend(b"System\0".ljust(30, b"\0"))
    packet.extend(payload)
    assert len(packet) == length
    # A leading zero selects Sphere's hexadecimal byte syntax.  Without it,
    # tokens containing A-F are treated as decimal prefixes by legacy parsing.
    return " ".join(f"0{value:02x}" for value in packet)


def _scripts() -> str:
    first = _system_packet_tokens("progress one")
    second = _system_packet_tokens("progress two")
    real_speech = "\n".join(
        f'  SAY("{speech}")' for speech in REAL_SPEECH_OPTIONS
    )
    return f"""

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

[FUNCTION f_tutorial_reward]
f_sysmessagecol(057,"<ARGV(2)>")
RETURN 0

[FUNCTION f_classmessage]
// Match the helper's second dispatch layer: the caller passes the expanded
// quoted argument and the callee forwards its raw ARGS value to the packet
// builder.
f_sysmessagecol(057,"<ARGS>")
RETURN 0

[FUNCTION tutAddActionXP]
// Keep the legacy helper's object-root call form in this fixture.  The
// helper forwards its expanded argument through the class-message wrapper.
f_classmessage(<ARGV(2)>)
RETURN 0

[CHARDEF {NPC_DEFNAME}]
DEFNAME={NPC_DEFNAME}
ID=0x0190
NAME=synthetic speech helper
NPC=brain_human
CAN=MT_USEHANDS
MOVERATE=0
STR=100
DEX=100
INT=100
ON=@UserDClick
IF (<SRC.ISGM>)
  RETURN 0
ENDIF
SRC.SENDPACKET {first}
SRC.SENDPACKET {second}
SRC.tutAddActionXP(20,greetedNPCdClick,\"{REAL_NOTICE}\")
DORAND 3
{real_speech}
ENDDO
RETURN 1

[EVENTS e_SpeechPacketProbe]
ON=@LogIn
NEWNPC {NPC_DEFNAME}
LASTNEW.P=129,128,0
RETURN 0
"""


def generate(output: Path) -> int:
    """Generate one player and one double-clickable NPC speech helper."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    text, count = re.subn(
        r"\n\[EVENTS e_AllPlayers\]\n.*?(?=\n\[FUNCTION f_arg_local_inner\])",
        "\n[EVENTS e_AllPlayers]\n",
        text,
        count=1,
        flags=re.DOTALL,
    )
    if count != 1:
        raise RuntimeError("synthetic login event was not found exactly once")
    tables.write_text(text + _scripts(), encoding="ascii")

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"""[ACCOUNT {ACCOUNT}]
PASSWORD={PASSWORD}
LASTCHARUID={CHAR_SERIAL}
CHARUID={CHAR_SERIAL}
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "sphereworld.scp").write_text(
        """TITLE=Sphere synthetic speech packet fixture
VERSION=0.99
SAVECOUNT=0
[EOF]
""",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        f"""TITLE=Sphere synthetic speech packet fixture
VERSION=0.99
SAVECOUNT=0
[WORLDCHAR c_MAN]
SERIAL={CHAR_SERIAL}
ACCOUNT={ACCOUNT}
NAME=SpeechPacketProbe
STR=100
INT=100
DEX=100
HITS=100
MAXHITS=100
MANA=100
STAM=100
P=128,128,0
EVENTS=e_SpeechPacketProbe
[EOF]
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="speech-packet",
        fixture_args=(),
        order=85,
        id_block=85,
        case=FixtureCase(
            name="speech-packet",
            mode="speech-packet",
            tests=(TestCase("test_speech_packet.py", (), True, True),),
            ports={"native": 2941, "asan": 2942},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="speech-packet",
            test_args_by_variant={},
        ),
    )
)
