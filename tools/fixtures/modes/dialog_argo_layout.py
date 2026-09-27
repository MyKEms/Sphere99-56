"""Registered synthetic fixture mode: dialog-argo-layout."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='dialog-argo-layout',
        fixture_args=('--dialog-argo-layout-probe',),
        order=46,
        id_block=44,
        case=FixtureCase(
            name='dialog-argo-layout',
            mode='dialog-argo-layout',
            tests=(TestCase('test_dialog_buttons.py', ('--argo-layout',), True, True),),
            ports={'native': 2736, 'asan': 2736},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='dialog-argo-layout',
            test_args_by_variant={},
        ),
    )
)
