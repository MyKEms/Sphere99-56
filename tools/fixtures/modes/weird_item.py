"""Registered synthetic fixture mode: weird-item."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='weird-item',
        fixture_args=('--world-load-counts', '--weird-item'),
        order=11,
        id_block=11,
        case=FixtureCase(
            name='weird-item',
            mode='weird-item',
            tests=(TestCase('test_world_load_counts.py', ('--weird-item',), True, True),),
            ports={'native': 2723, 'asan': 2723},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-weird-item',
            test_args_by_variant={},
        ),
    )
)
