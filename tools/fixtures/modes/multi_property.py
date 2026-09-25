"""Registered synthetic fixture mode: multi-property."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='multi-property',
        fixture_args=('--world-load-counts', '--multi-property'),
        order=14,
        id_block=14,
        case=FixtureCase(
            name='multi-property',
            mode='multi-property',
            tests=(TestCase('test_world_load_counts.py', ('--multi-property',), True, True),),
            ports={'native': 2718, 'asan': 2718},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-multi-property',
            test_args_by_variant={},
        ),
    )
)
