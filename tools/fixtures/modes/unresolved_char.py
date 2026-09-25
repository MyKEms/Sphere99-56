"""Registered synthetic fixture mode: unresolved-char."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unresolved-char',
        fixture_args=('--world-load-counts', '--world-load-counts-probe', '--unresolved-worldchar-type'),
        order=9,
        id_block=9,
        case=FixtureCase(
            name='unresolved-char',
            mode='unresolved-char',
            tests=(TestCase('test_world_load_counts.py', ('--unresolved-worldchar-type',), True, True),),
            ports={'native': 2715, 'asan': 2714},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-unresolved-char',
            test_args_by_variant={},
        ),
    )
)
