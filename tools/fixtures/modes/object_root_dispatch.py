"""Registered synthetic fixture mode: object-root script dispatch."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="object-root-dispatch",
        fixture_args=("--object-root-dispatch-probe",),
        # Keep this mode in the next free block after current master. Recheck
        # the registry before adding another mode.
        order=75,
        id_block=75,
        case=FixtureCase(
            name="object-root-dispatch",
            mode="object-root-dispatch",
            tests=(TestCase("test_object_root_dispatch.py", (), True, True),),
            ports={"native": 2868, "asan": 2868},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="object-root-dispatch",
            test_args_by_variant={},
        ),
    )
)
