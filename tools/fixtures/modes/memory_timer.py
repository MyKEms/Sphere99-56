"""Registered synthetic fixture mode: memory-timer."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="memory-timer",
        fixture_args=("--memory-timer-probe",),
        order=53,
        id_block=53,
        case=FixtureCase(
            name="memory-timer",
            mode="memory-timer",
            tests=(TestCase("test_memory_timer.py", (), True, True),),
            ports={"native": 2800, "asan": 2800},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="memory-timer",
            test_args_by_variant={},
        ),
    )
)
