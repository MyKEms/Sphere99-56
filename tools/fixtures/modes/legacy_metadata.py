"""Registered synthetic fixture mode: legacy-metadata."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


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
