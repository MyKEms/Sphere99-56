"""Registered synthetic fixture mode: format-compat-counts."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='format-compat-counts',
        fixture_args=('--world-load-counts', '--format-compat-probe', '--world-save-probe', '--roundtrip-integrity-probe'),
        order=19,
        id_block=19,
        case=FixtureCase(
            name='format-compat-counts',
            mode='format-compat-counts',
            tests=(TestCase('test_world_load_counts.py', ('--format-compat',), True, True),),
            ports={'native': 2728, 'asan': 2728},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='format-compat-counts',
            test_args_by_variant={},
        ),
    )
)
