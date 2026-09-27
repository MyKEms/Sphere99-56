"""Registered synthetic fixture mode: the bare EVENTS method."""

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="events-method",
        fixture_args=("--events-method-probe",),
        # Keep this pair above the newest registered synthetic ID blocks;
        # recheck it whenever the manifest gains another mode.
        order=61,
        id_block=63,
        case=FixtureCase(
            name="events-method",
            mode="events-method",
            tests=(TestCase("test_events_method.py", (), True, True),),
            ports={"native": 2862, "asan": 2863},
            output="events-method",
        ),
    )
)
