"""Registered synthetic fixture mode: runaway-loop."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="runaway-loop",
        fixture_args=("--runaway-loop-probe",),
        order=57,
        id_block=58,
        case=FixtureCase(
            name="runaway-loop",
            mode="runaway-loop",
            tests=(TestCase("test_runaway_loop.py", (), True, True),),
            ports={"native": 2754, "asan": 2755},
            output="runaway-loop",
        ),
    )
)
