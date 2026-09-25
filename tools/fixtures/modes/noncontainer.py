"""Registered synthetic fixture mode: noncontainer."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='noncontainer',
        fixture_args=('--world-load-counts', '--noncontainer-reference'),
        order=12,
        id_block=12,
        case=FixtureCase(
            name='noncontainer',
            mode='noncontainer',
            tests=(TestCase('test_world_load_counts.py', ('--noncontainer-reference',), True, True),),
            ports={'native': 2716, 'asan': 2716},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-counts-noncontainer',
            test_args_by_variant={},
        ),
    )
)
