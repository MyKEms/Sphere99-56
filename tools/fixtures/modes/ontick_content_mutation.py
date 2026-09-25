"""Registered synthetic fixture mode: ontick-content-mutation."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='ontick-content-mutation',
        fixture_args=('--ontick-content-mutation-probe',),
        order=33,
        id_block=33,
        case=FixtureCase(
            name='ontick-content-mutation',
            mode='ontick-content-mutation',
            tests=(TestCase('test_ontick_content_mutation.py', (), True, True),),
            ports={'native': 2799, 'asan': 2799},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='ontick-content-mutation',
            test_args_by_variant={},
        ),
    )
)
