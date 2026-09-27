"""Registered synthetic fixture mode: dotted-expressions."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

from .fragments.dotted_expressions import (
    DOTTED_CONDITION_ROWS,
    DOTTED_EXPRESSION_ROWS,
    DOTTED_PROBE_ACCOUNT,
    DOTTED_PROBE_CAPPED_FOR,
    DOTTED_PROBE_CAPPED_WHILE,
    DOTTED_PROBE_DISPOSABLE_ID,
    DOTTED_PROBE_FINDID_ID,
    DOTTED_PROBE_ITEM_ID,
    DOTTED_PROBE_LAYER,
    DOTTED_PROBE_MARKER,
    DOTTED_PROBE_SECTOR_LIGHT,
)

def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


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
