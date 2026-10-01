"""Registered synthetic fixture for refused plane-5 placement during walking."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .movement_stairs import MODE as STAIRS_MODE
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output: Path) -> int:
    """Start with the movement fixture, then place its player on map plane 5.

    The destination area is deliberately only on the next tile.  The player
    starts outside any area, so walk validation accepts the destination while
    the client resource-level check refuses the actual placement.
    """

    result = generate_recipe(output, STAIRS_MODE)
    if result:
        return result

    chars = output / "save" / "spherechars.scp"
    text = chars.read_text(encoding="ascii")
    if text.count("P=128,128,0\n") != 1:
        raise RuntimeError("plane walk fixture did not contain the stair player")
    chars.write_text(text.replace("P=128,128,0\n", "P=128,128,0,5\n"), encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + """
[AREA Synthetic plane 5 walk target]
P=128,127,0,5
RECT=128,126,129,128
""",
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="plane-walk-crash",
        fixture_args=(),
        order=82,
        id_block=82,
        case=FixtureCase(
            name="plane-walk-crash",
            mode="plane-walk-crash",
            tests=(TestCase("test_plane_walk_crash.py", (), True, True),),
            ports={"native": 2937, "asan": 2938},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="plane-walk-crash",
            test_args_by_variant={},
        ),
    )
)
