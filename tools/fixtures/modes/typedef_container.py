"""Registered synthetic fixture mode: typedef-container."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='typedef-container',
        fixture_args=('--world-load-counts', '--typedef-container-reference'),
        order=13,
        id_block=13,
        case=FixtureCase(
            name='typedef-container',
            mode='typedef-container',
            tests=(TestCase('test_world_load_counts.py', ('--typedef-container-reference',), True, True),),
            ports={'native': 2717, 'asan': 2717},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-typedef-container',
            test_args_by_variant={},
        ),
    )
)
