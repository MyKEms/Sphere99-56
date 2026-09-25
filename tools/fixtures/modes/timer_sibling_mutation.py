"""Registered synthetic fixture mode: timer-sibling-mutation."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='timer-sibling-mutation',
        fixture_args=('--timer-sibling-mutation-probe',),
        order=30,
        id_block=30,
        case=FixtureCase(
            name='timer-sibling-mutation',
            mode='timer-sibling-mutation',
            tests=(TestCase('test_timer_sibling_mutation.py', (), True, True),),
            ports={'native': 2796, 'asan': 2796},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='timer-sibling-mutation',
            test_args_by_variant={},
        ),
    )
)
