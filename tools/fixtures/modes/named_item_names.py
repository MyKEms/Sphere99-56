"""Registered synthetic fixture mode: named-item-names."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='named-item-names',
        fixture_args=('--world-load-counts', '--named-item-names'),
        order=15,
        id_block=15,
        case=FixtureCase(
            name='named-item-names',
            mode='named-item-names',
            tests=(TestCase('test_item_name_lifetime.py', (), True, True),),
            ports={'native': 2724, 'asan': 2724},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='named-item-names',
            test_args_by_variant={},
        ),
    )
)
