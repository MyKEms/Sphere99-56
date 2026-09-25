"""Registered synthetic fixture mode: unknown-overflow."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-overflow',
        fixture_args=('--unknown-keyword-report', '--unknown-keyword-overflow-probe'),
        order=25,
        id_block=25,
        case=FixtureCase(
            name='unknown-overflow',
            mode='unknown-overflow',
            tests=(TestCase('test_unknown_keyword_report.py', ('--overflow',), True, True),),
            ports={'native': 2696, 'asan': 2696},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='overflow',
            test_args_by_variant={},
        ),
    )
)
