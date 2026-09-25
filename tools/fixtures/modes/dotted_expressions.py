"""Registered synthetic fixture mode: dotted-expressions."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='dotted-expressions',
        fixture_args=('--dotted-expression-probe', '--unknown-keyword-report'),
        order=39,
        id_block=38,
        case=FixtureCase(
            name='dotted-expressions',
            mode='dotted-expressions',
            tests=(TestCase('test_dotted_expressions.py', (), True, True),),
            ports={'native': 2730, 'asan': 2731},
            mode_by_variant={},
            generator='make_fixture.py',
            generator_args=(),
            output='dotted-expressions',
            test_args_by_variant={},
        ),
    )
)
