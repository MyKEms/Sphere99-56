"""Registered synthetic fixture mode: lifetime."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='lifetime',
        fixture_args=(),
        order=27,
        id_block=27,
        case=FixtureCase(
            name='lifetime',
            mode='lifetime',
            tests=(TestCase('run_suite.py', ('--lifetime-soak', '25'), True, True),),
            ports={'native': 2793, 'asan': 2793},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='lifetime',
            test_args_by_variant={},
        ),
    )
)
