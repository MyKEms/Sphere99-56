"""Registered synthetic fixture mode: trigger-recursion."""

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="trigger-recursion",
        fixture_args=("--trigger-recursion-probe",),
        order=201,
        id_block=152,
        case=FixtureCase(
            name="trigger-recursion",
            mode="trigger-recursion",
            tests=(TestCase("test_trigger_recursion.py", (), True, True),),
            ports={"native": 6400, "asan": 9400},
            output="trigger-recursion",
        ),
    )
)
