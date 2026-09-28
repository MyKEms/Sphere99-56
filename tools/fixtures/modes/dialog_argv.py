"""Registered synthetic fixture mode: dialog-argv."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="dialog-argv",
        fixture_args=("--dialog-argv-probe",),
        order=67,
        id_block=67,
        case=FixtureCase(
            name="dialog-argv",
            mode="dialog-argv",
            tests=(TestCase("test_dialog_argv.py", (), True, True),),
            ports={"native": 2864, "asan": 2865},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="dialog-argv",
            test_args_by_variant={},
        ),
    )
)
