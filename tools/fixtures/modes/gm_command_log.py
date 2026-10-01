"""Registered synthetic fixture mode: GM command logs and console input."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="gm-command-log",
        fixture_args=("--gm-command-log-probe",),
        order=78,
        id_block=78,
        case=FixtureCase(
            name="gm-command-log",
            mode="gm-command-log",
            tests=(TestCase("test_gm_command_log.py", (), True, True),),
            ports={"native": 2884, "asan": 2885},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="gm-command-log",
            test_args_by_variant={},
        ),
    )
)
