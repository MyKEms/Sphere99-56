"""Registered synthetic fixture mode: spawn-gem."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

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
