"""Registered synthetic fixture mode: dialog-flow-layout."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='dialog-flow-layout',
        fixture_args=('--dialog-flow-layout-probe',),
        order=47,
        id_block=45,
        case=FixtureCase(
            name='dialog-flow-layout',
            mode='dialog-flow-layout',
            tests=(TestCase('test_dialog_buttons.py', ('--flow-layout',), True, True),),
            ports={'native': 2737, 'asan': 2737},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='dialog-flow-layout',
            test_args_by_variant={},
        ),
    )
)
