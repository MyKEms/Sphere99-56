"""Registered synthetic fixture mode: world-counts."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='world-counts',
        fixture_args=('--world-load-counts', '--world-load-counts-probe'),
        order=7,
        id_block=7,
        case=FixtureCase(
            name='world-counts',
            mode='world-counts',
            tests=(TestCase('test_world_load_counts.py', (), True, True),),
            ports={'native': 2712, 'asan': 2710},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts',
            test_args_by_variant={},
        ),
    )
)
