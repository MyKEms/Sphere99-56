"""Registered synthetic fixture mode: unknown-report."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='unknown-report',
        fixture_args=('--unknown-keyword-report',),
        order=151,
        id_block=2,
    )
)
