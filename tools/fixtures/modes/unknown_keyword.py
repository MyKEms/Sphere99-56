"""Registered synthetic fixture mode: unknown-keyword."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-keyword',
        fixture_args=('--unknown-keyword-report', '--unknown-keyword-probe'),
        order=0,
        id_block=3,
        case=FixtureCase(
            name='unknown-keyword',
            mode='unknown-keyword',
            tests=(TestCase('test_unknown_keyword_report.py', (), True, True),),
            ports={'native': 2693, 'asan': 2693},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='probe',
            test_args_by_variant={},
        ),
    )
)
