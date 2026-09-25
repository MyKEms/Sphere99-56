"""Registered synthetic fixture mode: unknown-disabled."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-disabled',
        fixture_args=('--unknown-keyword-probe',),
        order=3,
        id_block=6,
        case=FixtureCase(
            name='unknown-disabled',
            mode='unknown-disabled',
            tests=(TestCase('test_unknown_keyword_report.py', ('--expect-disabled',), True, True),),
            ports={'native': 2695, 'asan': 2695},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='disabled',
            test_args_by_variant={},
        ),
    )
)
