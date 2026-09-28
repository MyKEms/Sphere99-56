"""Registered synthetic fixture mode: deferred party teardown."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="party-lifetime",
        fixture_args=(),
        order=65,
        id_block=65,
        case=FixtureCase(
            name="party-lifetime",
            mode="party-lifetime",
            tests=(TestCase("test_party_lifetime.py", (), True, True),),
            ports={"native": 2864, "asan": 2864},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="party-lifetime",
            test_args_by_variant={},
        ),
    )
)
