"""Registered synthetic fixture mode: dword-hex."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='dword-hex',
        fixture_args=('--dword-hex-probe',),
        order=42,
        id_block=40,
        case=FixtureCase(
            name='dword-hex',
            mode='dword-hex',
            tests=(TestCase('test_dword_hex.py', (), True, True),),
            ports={'native': 2739, 'asan': 2738},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='dword-hex',
            test_args_by_variant={},
        ),
    )
)
