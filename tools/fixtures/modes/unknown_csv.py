"""Registered synthetic fixture mode: unknown-csv."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-csv',
        fixture_args=('--unknown-keyword-report', '--unknown-keyword-report-format', 'csv', '--unknown-keyword-probe'),
        order=24,
        id_block=24,
        case=FixtureCase(
            name='unknown-csv',
            mode='unknown-csv',
            tests=(TestCase('test_unknown_keyword_report.py', ('--csv',), True, True),),
            ports={'native': 2697, 'asan': 2697},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='csv',
            test_args_by_variant={},
        ),
    )
)
