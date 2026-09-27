"""Registered synthetic fixture mode: character FOOD and item context."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="food",
        fixture_args=("--food-probe", "--unknown-keyword-report"),
        order=44,
        id_block=60,
        case=FixtureCase(
            name="food",
            mode="food",
            tests=(TestCase("test_food.py", (), True, True),),
            ports={"native": 2762, "asan": 2763},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="food",
            test_args_by_variant={},
        ),
    )
)
