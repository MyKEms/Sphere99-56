"""Registered synthetic fixture mode: legacy-metadata."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='legacy-metadata',
        fixture_args=('--legacy-metadata-probe',),
        order=22,
        id_block=22,
        case=FixtureCase(
            name='legacy-metadata',
            mode='legacy-metadata',
            tests=(TestCase('test_legacy_metadata_roundtrip.py', (), True, True),),
            ports={'native': 2742, 'asan': 2743},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='legacy-metadata',
            test_args_by_variant={},
        ),
    )
)
