"""Registered synthetic fixture mode: same-definition item stacking."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="movement-stacking",
        fixture_args=("--movement-stacking-probe",),
        order=54,
        id_block=54,
        case=FixtureCase(
            name="movement-stacking",
            mode="movement-stacking",
            tests=(TestCase("test_item_stacking.py", (), True, True),),
            ports={"native": 2852, "asan": 2853},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="movement-stacking",
            test_args_by_variant={},
        ),
    )
)
