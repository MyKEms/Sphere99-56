"""Registered synthetic fixture mode: arg-locals."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='arg-locals',
        fixture_args=('--arg-locals-probe',),
        order=40,
        id_block=39,
        case=FixtureCase(
            name='arg-locals',
            mode='arg-locals',
            tests=(TestCase('test_arg_locals.py', (), True, True),),
            ports={'native': 2732, 'asan': 2732},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='arg-locals',
            test_args_by_variant={},
        ),
    )
)
