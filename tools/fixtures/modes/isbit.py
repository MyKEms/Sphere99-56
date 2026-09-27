"""Registered synthetic fixture mode: isbit."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)

MODE = register_mode(
    FixtureMode(
        name="isbit",
        fixture_args=("--isbit-probe",),
        order=43,
        id_block=59,
        case=FixtureCase(
            name="isbit",
            mode="isbit",
            tests=(TestCase("test_isbit.py", (), True, True),),
            ports={"native": 2760, "asan": 2761},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="isbit",
            test_args_by_variant={},
        ),
    )
)
