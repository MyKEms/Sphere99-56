"""Registered synthetic fixture mode: dynamic stair movement."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="movement-stairs",
        fixture_args=("--movement-stairs-probe",),
        order=55,
        id_block=54,
        case=FixtureCase(
            name="movement-stairs",
            mode="movement-stairs",
            tests=(TestCase("test_movement_stairs.py", (), True, True),),
            ports={"native": 2799, "asan": 2799},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="movement-stairs",
            test_args_by_variant={},
        ),
    )
)
