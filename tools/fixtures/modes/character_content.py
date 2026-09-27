"""Registered synthetic fixture mode: direct character-owned item."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="character-content",
        fixture_args=("--world-load-counts", "--character-content-probe"),
        order=55,
        # 55 belongs to the stairs mode on current master; keep this mode in
        # its own synthetic-id block after the merge.
        id_block=57,
        case=FixtureCase(
            name="character-content",
            mode="character-content",
            tests=(TestCase("test_character_content.py", (), True, True),),
            ports={"native": 2755, "asan": 2755},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="world-counts-character-content",
            test_args_by_variant={},
        ),
    )
)
