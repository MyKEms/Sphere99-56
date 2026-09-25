"""Registered synthetic fixture mode: world-roundtrip."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='world-roundtrip',
        fixture_args=('--world-load-counts', '--child-before-parent', '--world-save-probe', '--roundtrip-integrity-probe'),
        order=18,
        id_block=18,
        case=FixtureCase(
            name='world-roundtrip',
            mode='world-roundtrip',
            tests=(TestCase('test_world_roundtrip_integrity.py', (), True, True),),
            ports={'native': 2727, 'asan': 2727},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-roundtrip',
            test_args_by_variant={},
        ),
    )
)
