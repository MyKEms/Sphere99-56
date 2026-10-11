"""Registered synthetic fixture mode: escape-overflow."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='escape-overflow',
        fixture_args=('--escape-overflow-probe',),
        order=49,
        id_block=47,
        case=FixtureCase(
            name='escape-overflow',
            mode='escape-overflow',
            tests=(TestCase('test_escape_overflow.py'),),
            # Keep the game port distinct from container-shutdown when shards run sequentially.
            ports={'native': 2806, 'asan': 2806},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='escape-overflow',
            test_args_by_variant={},
        ),
    )
)
