"""Registered synthetic fixture mode: coverage-shutdown."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='coverage-shutdown',
        fixture_args=None,
        order=4,
        id_block=48,
        case=FixtureCase(
            name='coverage-shutdown',
            mode=None,
            tests=(TestCase('test_script_execution_coverage.py', (), True, True),),
            ports={'native': 2700, 'asan': 2700},
            mode_by_variant={},
            generator='make_script_coverage_fixture.py',
            generator_args=(),
            output='coverage-shutdown',
            test_args_by_variant={},
        ),
    )
)
