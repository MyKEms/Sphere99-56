"""Registered synthetic fixture mode: daily logging sinks."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="daily-logging",
        fixture_args=("--daily-logging-probe",),
        order=72,
        id_block=72,
        case=FixtureCase(
            name="daily-logging",
            mode="daily-logging",
            tests=(TestCase("test_daily_logging.py", (), True, True),),
            ports={"native": 2805, "asan": 2805},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="daily-logging",
            test_args_by_variant={},
        ),
    )
)
