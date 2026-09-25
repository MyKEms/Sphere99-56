"""Registered synthetic fixture mode: coverage-disabled."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='coverage-disabled',
        fixture_args=None,
        order=6,
        id_block=50,
        case=FixtureCase(
            name='coverage-disabled',
            mode=None,
            tests=(TestCase('test_script_execution_coverage.py', ('--expect-disabled',), True, True),),
            ports={'native': 2702, 'asan': 2702},
            mode_by_variant={},
            generator='make_script_coverage_fixture.py',
            generator_args=('--disabled', '--on-demand-report'),
            output='coverage-disabled',
            test_args_by_variant={},
        ),
    )
)
