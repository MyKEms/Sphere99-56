"""Registered synthetic fixture mode: dialog-buttons."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='dialog-buttons',
        fixture_args=('--dialog-button-probe',),
        order=45,
        id_block=43,
        case=FixtureCase(
            name='dialog-buttons',
            mode='dialog-buttons',
            tests=(TestCase('test_dialog_buttons.py', (), True, True),),
            ports={'native': 2734, 'asan': 2734},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='dialog-buttons',
            test_args_by_variant={},
        ),
    )
)
