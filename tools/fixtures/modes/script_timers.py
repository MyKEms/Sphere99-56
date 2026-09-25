"""Registered synthetic fixture mode: script-timers."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="script-timers",
        fixture_args=("--script-timer-probe",),
        order=54,
        id_block=54,
        case=FixtureCase(
            name="script-timers",
            mode="script-timers",
            tests=(TestCase("test_script_timers.py", (), True, True),),
            ports={"native": 2800, "asan": 2800},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="script-timers",
            test_args_by_variant={},
        ),
    )
)
