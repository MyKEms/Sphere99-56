"""Registered synthetic fixture mode: gump-fallback."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate the no-gump container save through the compatibility writer."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="gump-fallback",
        fixture_args=(
            "--world-load-counts",
            "--gump-fallback-probe",
            "--world-save-probe",
            "--roundtrip-integrity-probe",
        ),
        order=62,
        id_block=64,
        case=FixtureCase(
            name="gump-fallback",
            mode="gump-fallback",
            tests=(
                TestCase("test_world_load_counts.py", ("--gump-fallback",), True, True),
                TestCase(
                    "test_world_roundtrip_integrity.py",
                    ("--gump-fallback",),
                    True,
                    True,
                ),
            ),
            ports={"native": 2762, "asan": 2762},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="gump-fallback",
            test_args_by_variant={},
        ),
    )
)
