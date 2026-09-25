"""Registered synthetic fixture mode: gump-skill."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='gump-skill',
        fixture_args=None,
        order=48,
        id_block=52,
        case=FixtureCase(
            name='gump-skill',
            mode=None,
            tests=(TestCase('test_gump_skill.py', (), True, True),),
            ports={'native': 2735, 'asan': 2735},
            mode_by_variant={},
            generator='make_gump_skill_fixture.py',
            generator_args=(),
            output='gump-skill',
            test_args_by_variant={},
        ),
    )
)
