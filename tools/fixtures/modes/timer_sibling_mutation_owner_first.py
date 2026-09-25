"""Registered synthetic fixture mode: timer-sibling-mutation-owner-first."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='timer-sibling-mutation-owner-first',
        fixture_args=('--timer-sibling-mutation-owner-first-probe',),
        order=31,
        id_block=31,
        case=FixtureCase(
            name='timer-sibling-mutation-owner-first',
            mode='timer-sibling-mutation-owner-first',
            tests=(TestCase('test_timer_sibling_mutation.py', (), True, True),),
            ports={'native': 2797, 'asan': 2797},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='timer-sibling-mutation-owner-first',
            test_args_by_variant={},
        ),
    )
)
