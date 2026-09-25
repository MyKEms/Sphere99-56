"""Registered synthetic fixture mode: events-attr."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='events-attr',
        fixture_args=('--events-attr-probe',),
        order=21,
        id_block=21,
        case=FixtureCase(
            name='events-attr',
            mode='events-attr',
            tests=(TestCase('test_events_attr_roundtrip.py', (), True, True),),
            ports={'native': 2735, 'asan': 2735},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='events-attr',
            test_args_by_variant={},
        ),
    )
)
