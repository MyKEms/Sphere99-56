"""Registered synthetic fixture mode: damage-triggers."""

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="damage-triggers",
        fixture_args=("--damage-trigger-probe",),
        order=58,
        id_block=60,
        case=FixtureCase(
            name="damage-triggers",
            mode="damage-triggers",
            tests=(TestCase("test_damage_triggers.py", (), True, True),),
            ports={"native": 2860, "asan": 2861},
            output="damage-triggers",
        ),
    )
)
