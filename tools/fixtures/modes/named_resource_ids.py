"""Registered synthetic fixture mode: named-resource-ids."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='named-resource-ids',
        fixture_args=('--world-load-counts', '--named-resource-ids'),
        order=16,
        id_block=16,
        case=FixtureCase(
            name='named-resource-ids',
            mode='named-resource-ids',
            tests=(TestCase('test_named_resource_ids.py', (), True, False), TestCase('test_world_load_counts.py', ('--named-container-reference',), True, True)),
            ports={'native': 2719, 'asan': 2719},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='named-resource-ids',
            test_args_by_variant={},
        ),
    )
)
