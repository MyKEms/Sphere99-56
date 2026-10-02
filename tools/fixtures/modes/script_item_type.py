"""Registered synthetic fixture mode: script-defined item TYPE=0."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="script-item-type",
        fixture_args=(
            "--world-load-counts",
            "--world-load-counts-probe",
            "--script-item-type-probe",
        ),
        order=91,
        id_block=91,
        case=FixtureCase(
            name="script-item-type",
            mode="script-item-type",
            tests=(
                TestCase(
                    "test_world_load_counts.py",
                    ("--script-item-type-reference",),
                    True,
                    True,
                ),
            ),
            ports={"native": 2791, "asan": 2791},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="world-counts-script-item-type",
            test_args_by_variant={},
        ),
    )
)
