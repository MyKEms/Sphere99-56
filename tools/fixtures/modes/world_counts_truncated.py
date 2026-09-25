"""Registered synthetic fixture mode: world-counts-truncated."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='world-counts-truncated',
        fixture_args=('--world-load-counts', '--world-load-counts-probe', '--truncate-world-item'),
        order=8,
        id_block=8,
        case=FixtureCase(
            name='world-counts-truncated',
            mode='world-counts-truncated',
            tests=(TestCase('test_world_load_counts.py', ('--truncated',), True, True),),
            ports={'native': 2713, 'asan': 2711},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-truncated',
            test_args_by_variant={},
        ),
    )
)
