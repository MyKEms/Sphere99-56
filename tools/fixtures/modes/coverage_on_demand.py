"""Registered synthetic fixture mode: coverage-on-demand."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='coverage-on-demand',
        fixture_args=None,
        order=5,
        id_block=49,
        case=FixtureCase(
            name='coverage-on-demand',
            mode=None,
            tests=(TestCase('test_script_execution_coverage.py', ('--expect-on-demand',), True, True),),
            ports={'native': 2701, 'asan': 2701},
            mode_by_variant={},
            generator='make_script_coverage_fixture.py',
            generator_args=('--on-demand-report',),
            output='coverage-on-demand',
            test_args_by_variant={},
        ),
    )
)
