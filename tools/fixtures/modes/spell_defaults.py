"""Registered synthetic fixture mode: spell-defaults."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name="spell-defaults",
        fixture_args=("--spell-defaults-probe",),
        order=53,
        id_block=53,
        case=FixtureCase(
            name="spell-defaults",
            mode="spell-defaults",
            tests=(TestCase("test_spell_defaults.py", (), True, True),),
            ports={"native": 2802, "asan": 2800},
            output="spell-defaults",
        ),
    )
)
