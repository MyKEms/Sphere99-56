"""Registered synthetic fixture mode: world-save."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='world-save',
        fixture_args=('--world-save-probe',),
        order=34,
        id_block=34,
        case=FixtureCase(
            name='world-save',
            mode='world-save',
            tests=(TestCase('test_world_save_roundtrip.py', (), True, True),),
            ports={'native': 2720, 'asan': 2720},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='world-save',
            test_args_by_variant={},
        ),
    )
)
