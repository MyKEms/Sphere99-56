"""Registered synthetic fixture mode: unknown-rejected."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-rejected',
        fixture_args=('--unknown-keyword-report', '--unknown-keyword-rejected-probe'),
        order=2,
        id_block=5,
        case=FixtureCase(
            name='unknown-rejected',
            mode='unknown-rejected',
            tests=(TestCase('test_unknown_keyword_report.py', ('--rejected',), True, True),),
            ports={'native': 2698, 'asan': 2698},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='rejected',
            test_args_by_variant={},
        ),
    )
)
