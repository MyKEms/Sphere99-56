"""Registered synthetic fixture mode: spawn-gem-duplicate."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='spawn-gem-duplicate',
        fixture_args=('--spawn-gem-duplicate-serial-probe',),
        order=37,
        id_block=36,
        case=FixtureCase(
            name='spawn-gem-duplicate',
            mode='spawn-gem-duplicate',
            tests=(TestCase('test_spawn_gem_serialization.py', ('--duplicate-load',), True, True),),
            ports={'native': 2743, 'asan': 2741},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='spawn-gem-duplicate',
            test_args_by_variant={},
        ),
    )
)
