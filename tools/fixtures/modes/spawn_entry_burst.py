"""Synthetic coverage for entry ordering with a dense transient-item view."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT = "SpawnEntryProbe"
PASSWORD = "spawnpw"
EFFECT_COUNT = 900
EFFECT_SERIAL_BASE = 0x40001000


def generate(output: Path) -> int:
    """Create a player entry point surrounded by visible short-lived effects."""

    result = generate_recipe(output, BASE_MODE)
    if result:
        return result

    (output / "accounts" / "sphereaccu.scp").write_text(
        f"[ACCOUNT {ACCOUNT}]\nPASSWORD={PASSWORD}\n[EOF]\n",
        encoding="ascii",
    )
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + "\n[ITEMDEF 0x375A]\nDEFNAME=i_fx_sparkle\nTYPE=T_NORMAL\n",
        encoding="ascii",
    )
    (output / "save" / "spherechars.scp").write_text(
        "TITLE=Sphere synthetic spawn-entry fixture\n"
        "VERSION=0.99\n"
        "SAVECOUNT=0\n"
        "[EOF]\n",
        encoding="ascii",
    )
    effects = []
    for index in range(EFFECT_COUNT):
        serial = EFFECT_SERIAL_BASE + index
        effects.append(
            "[WORLDITEM i_fx_sparkle]\n"
            f"SERIAL=0{serial:x}\n"
            "ATTR=attr_static|attr_decay\n"
            "TIMER=60\n"
            "COLOR=0995\n"
            "P=128,128,0\n"
        )
    (output / "save" / "sphereworld.scp").write_text(
        "TITLE=Sphere synthetic spawn-entry fixture\n"
        "VERSION=0.99\n"
        "SAVECOUNT=0\n"
        + "".join(effects)
        + "[EOF]\n",
        encoding="ascii",
    )
    return 0


BASE_MODE = FixtureMode(name="spawn-entry-burst-base", fixture_args=())


MODE = register_mode(
    FixtureMode(
        name="spawn-entry-burst",
        fixture_args=(),
        order=193,
        id_block=145,
        case=FixtureCase(
            name="spawn-entry-burst",
            mode="spawn-entry-burst",
            tests=(TestCase("test_spawn_entry_burst.py", (), True, True),),
            ports={"native": 3202, "asan": 3203},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="spawn-entry-burst",
            test_args_by_variant={},
        ),
    )
)
