"""Registered synthetic fixture mode: child-before-parent."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='child-before-parent',
        fixture_args=('--world-load-counts', '--child-before-parent'),
        order=17,
        id_block=17,
        case=FixtureCase(
            name='child-before-parent',
            mode='child-before-parent',
            tests=(TestCase('test_world_load_counts.py', ('--child-before-parent',), True, True),),
            ports={'native': 2724, 'asan': 2724},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-child-before-parent',
            test_args_by_variant={},
        ),
    )
)
