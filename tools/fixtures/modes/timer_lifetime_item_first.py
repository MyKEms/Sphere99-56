"""Registered synthetic fixture mode: timer-lifetime-item-first."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name='timer-lifetime-item-first',
        fixture_args=('--timer-lifetime-item-first-probe',),
        order=29,
        id_block=29,
        case=FixtureCase(
            name='timer-lifetime-item-first',
            mode='timer-lifetime-item-first',
            tests=(TestCase('test_timer_lifetime.py', ('--item-first',), True, True),),
            ports={'native': 2795, 'asan': 2795},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='timer-lifetime-item-first',
            test_args_by_variant={},
        ),
    )
)
