"""Registered synthetic fixture mode: recursion-depth."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="recursion-depth",
        fixture_args=("--recursion-depth-probe",),
        order=58,
        id_block=60,
        case=FixtureCase(
            name="recursion-depth",
            mode="recursion-depth",
            tests=(TestCase("test_recursion_depth.py", (), True, True),),
            ports={"native": 2802, "asan": 2802},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="recursion-depth",
            test_args_by_variant={},
        ),
    )
)
