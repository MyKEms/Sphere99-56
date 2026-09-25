"""Registered synthetic fixture mode: region-weather."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='region-weather',
        fixture_args=('--region-weather-probe', '--unknown-keyword-report'),
        order=43,
        id_block=41,
        case=FixtureCase(
            name='region-weather',
            mode='region-weather',
            tests=(TestCase('test_region_weather.py', (), True, True),),
            ports={'native': 2751, 'asan': 2750},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='region-weather',
            test_args_by_variant={},
        ),
    )
)
