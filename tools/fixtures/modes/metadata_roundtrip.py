"""Registered synthetic fixture mode: metadata-roundtrip."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='metadata-roundtrip',
        fixture_args=('--world-load-counts', '--metadata-roundtrip-probe', '--world-save-probe', '--roundtrip-integrity-probe'),
        order=23,
        id_block=23,
        case=FixtureCase(
            name='metadata-roundtrip',
            mode='metadata-roundtrip',
            tests=(TestCase('test_world_roundtrip_integrity.py', ('--metadata-roundtrip',), True, True),),
            ports={'native': 2745, 'asan': 2744},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='metadata-roundtrip',
            test_args_by_variant={},
        ),
    )
)
