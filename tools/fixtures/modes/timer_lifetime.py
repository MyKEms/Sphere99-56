"""Registered synthetic fixture mode: timer-lifetime."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='timer-lifetime',
        fixture_args=('--timer-lifetime-probe',),
        order=28,
        id_block=28,
        case=FixtureCase(
            name='timer-lifetime',
            mode='timer-lifetime',
            tests=(TestCase('test_timer_lifetime.py', (), True, True),),
            ports={'native': 2794, 'asan': 2794},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='timer-lifetime',
            test_args_by_variant={},
        ),
    )
)
