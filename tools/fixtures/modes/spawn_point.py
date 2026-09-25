"""Registered synthetic fixture mode: spawn-point."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='spawn-point',
        fixture_args=('--spawn-point-probe',),
        order=38,
        id_block=37,
        case=FixtureCase(
            name='spawn-point',
            mode='spawn-point',
            tests=(TestCase('test_spawn_point.py', (), True, True),),
            ports={'native': 2745, 'asan': 2744},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='spawn-point',
            test_args_by_variant={},
        ),
    )
)
