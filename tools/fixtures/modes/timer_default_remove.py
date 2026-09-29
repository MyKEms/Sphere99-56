"""Registered synthetic fixture mode: timer-default-remove."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="timer-default-remove",
        fixture_args=("--timer-default-remove-probe",),
        order=70,
        id_block=70,
        case=FixtureCase(
            name="timer-default-remove",
            mode="timer-default-remove",
            tests=(TestCase("test_timer_default_remove.py", (), True, True),),
            ports={"native": 2803, "asan": 2803},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="timer-default-remove",
            test_args_by_variant={},
        ),
    )
)
