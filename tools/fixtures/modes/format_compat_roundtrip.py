"""Registered synthetic fixture mode: format-compat-roundtrip."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='format-compat-roundtrip',
        fixture_args=('--world-load-counts', '--format-compat-probe', '--world-save-probe', '--roundtrip-integrity-probe'),
        order=20,
        id_block=20,
        case=FixtureCase(
            name='format-compat-roundtrip',
            mode='format-compat-roundtrip',
            tests=(TestCase('test_world_roundtrip_integrity.py', ('--format-compat',), True, True),),
            ports={'native': 2729, 'asan': 2729},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='format-compat-roundtrip',
            test_args_by_variant={},
        ),
    )
)
