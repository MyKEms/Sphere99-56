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
LONG_SPEECH = "Long synthetic tutorial speech " + ("abcdefghij" * 18)


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
    return f"""

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
SRC.SENDPACKET {first}
SRC.SENDPACKET {second}
SAY(\"{LONG_SPEECH}\")
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
