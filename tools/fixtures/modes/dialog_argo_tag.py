"""Registered synthetic fixture mode: dialog-argo-tag."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="dialog-argo-tag",
        fixture_args=("--dialog-argo-tag-probe",),
        order=64,
        id_block=65,
        case=FixtureCase(
            name="dialog-argo-tag",
            mode="dialog-argo-tag",
            tests=(TestCase("test_dialog_argo_tag.py", (), True, True),),
            ports={"native": 2866, "asan": 2867},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="dialog-argo-tag",
            test_args_by_variant={},
        ),
    )
)
