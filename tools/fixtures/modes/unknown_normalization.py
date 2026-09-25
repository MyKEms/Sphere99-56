"""Registered synthetic fixture mode: unknown-normalization."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-normalization',
        fixture_args=('--unknown-keyword-report', '--unknown-keyword-normalization-probe', '--unknown-keyword-set-probe', '--unknown-keyword-admin-probe'),
        order=1,
        id_block=4,
        case=FixtureCase(
            name='unknown-normalization',
            mode='unknown-normalization',
            tests=(TestCase('test_unknown_keyword_report.py', ('--normalization', '--expect-set', '--expect-on-demand'), True, True),),
            ports={'native': 2694, 'asan': 2694},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='normalization',
            test_args_by_variant={},
        ),
    )
)
