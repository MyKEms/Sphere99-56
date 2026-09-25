"""Registered synthetic fixture mode: fixture."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='fixture',
        fixture_args=('--unknown-newbie', '--unknown-keyword-report'),
        order=26,
        id_block=26,
        case=FixtureCase(
            name='fixture',
            mode='fixture',
            tests=(TestCase('run_suite.py', (), True, True),),
            ports={'native': 2593, 'asan': 2593},
            mode_by_variant={'asan': 'unknown-report'},
            generator='make_fixture.py',
            generator_args=(),
            output='fixture',
            test_args_by_variant={'native': ('--expect-invalid-newbie', 'SYNTHETIC_UNKNOWN_SKILL', '--unknown-keyword-allowlist'), 'asan': ('--unknown-keyword-allowlist',)},
        ),
    )
)
