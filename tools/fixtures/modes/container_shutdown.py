"""Registered synthetic fixture mode: container-shutdown."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='container-shutdown',
        fixture_args=('--container-shutdown-probe',),
        order=32,
        id_block=32,
        case=FixtureCase(
            name='container-shutdown',
            mode='container-shutdown',
            tests=(TestCase('test_container_shutdown.py', (), True, True),),
            ports={'native': 2798, 'asan': 2798},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='container-shutdown',
            test_args_by_variant={},
        ),
    )
)
