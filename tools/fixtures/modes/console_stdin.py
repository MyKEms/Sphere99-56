"""Registered synthetic fixture mode: non-TTY console input."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="console-stdin",
        fixture_args=("--console-stdin-probe",),
        order=174,
        id_block=121,
        case=FixtureCase(
            name="console-stdin",
            mode="console-stdin",
            tests=(TestCase("test_console_stdin.py", (), True, True),),
            ports={"native": 2890, "asan": 2891},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="console-stdin",
            test_args_by_variant={},
        ),
    )
)
