"""Registered synthetic fixture for walking on logical map plane 5."""

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .movement_stairs import MODE as STAIRS_MODE
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output: Path) -> int:
    """Start with the movement fixture, then place its player on map plane 5."""

    result = generate_recipe(output, STAIRS_MODE)
    if result:
        return result

    chars = output / "save" / "spherechars.scp"
    text = chars.read_text(encoding="ascii")
    if text.count("P=128,128,0\n") != 1:
        raise RuntimeError("plane walk fixture did not contain the stair player")
    chars.write_text(text.replace("P=128,128,0\n", "P=128,128,0,5\n"), encoding="ascii")

    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    if text.count("[AREA Synthetic world]\nP=128,128,0\n") != 1:
        raise RuntimeError("plane walk fixture did not contain its shared area")
    tables.write_text(
        text.replace(
            "[AREA Synthetic world]\nP=128,128,0\n",
            "[AREA Synthetic world]\nP=128,128,0,255\n",
        ),
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
