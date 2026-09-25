"""Registered synthetic fixture mode: findarg."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='findarg',
        fixture_args=('--findarg-probe',),
        order=41,
        id_block=46,
        case=FixtureCase(
            name='findarg',
            mode='findarg',
            tests=(TestCase('test_findarg.py'),),
            ports={'native': 2735, 'asan': 2735},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='findarg',
            test_args_by_variant={},
        ),
    )
)
