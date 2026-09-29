"""Registered synthetic fixture mode: 0.99 right-to-left expressions."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="expression-chain",
        fixture_args=("--expression-chain-probe",),
        order=74,
        id_block=74,
        case=FixtureCase(
            name="expression-chain",
            mode="expression-chain",
            tests=(TestCase("test_expression_chain.py", (), True, True),),
            ports={"native": 2804, "asan": 2804},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="expression-chain",
            test_args_by_variant={},
        ),
    )
)
