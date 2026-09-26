"""Registered synthetic fixture mode: direct character-owned item."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="character-content",
        fixture_args=("--world-load-counts", "--character-content-probe"),
        order=55,
        id_block=55,
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
