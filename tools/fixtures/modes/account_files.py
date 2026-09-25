"""Registered synthetic fixture mode: account-files."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='account-files',
        fixture_args=None,
        order=35,
        id_block=51,
        case=FixtureCase(
            name='account-files',
            mode=None,
            tests=(TestCase('test_account_files.py', (), False, True),),
            ports={'native': 2751, 'asan': 2750},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output=None,
            test_args_by_variant={},
        ),
    )
)
