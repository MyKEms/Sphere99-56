"""Registered synthetic fixture mode: dynamic and map teleporters."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="movement-teleporters",
        fixture_args=None,
        order=56,
        id_block=56,
        case=FixtureCase(
            name="movement-teleporters",
            mode=None,
            tests=(TestCase("test_teleporters.py", (), True, True),),
            ports={"native": 2801, "asan": 2801},
            mode_by_variant={},
            generator="make_teleporter_fixture.py",
            generator_args=(),
            output="teleporters",
            test_args_by_variant={},
        ),
    )
)
