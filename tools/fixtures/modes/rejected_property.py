"""Registered synthetic fixture mode: rejected-property."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='rejected-property',
        fixture_args=('--world-load-counts', '--rejected-property'),
        order=10,
        id_block=10,
        case=FixtureCase(
            name='rejected-property',
            mode='rejected-property',
            tests=(TestCase('test_world_load_counts.py', ('--rejected-property',), True, True),),
            ports={'native': 2722, 'asan': 2722},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-rejected-property',
            test_args_by_variant={},
        ),
    )
)
