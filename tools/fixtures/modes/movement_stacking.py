"""Registered synthetic fixture mode: explicit and no-point item stacking."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


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
