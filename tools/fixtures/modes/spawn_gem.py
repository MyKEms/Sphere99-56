"""Registered synthetic fixture mode: spawn-gem."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='spawn-gem',
        fixture_args=('--spawn-gem-probe', '--world-save-probe'),
        order=36,
        id_block=35,
        case=FixtureCase(
            name='spawn-gem',
            mode='spawn-gem',
            tests=(TestCase('test_spawn_gem_serialization.py', (), True, True),),
            ports={'native': 2742, 'asan': 2740},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='spawn-gem',
            test_args_by_variant={},
        ),
    )
)
