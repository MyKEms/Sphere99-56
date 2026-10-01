"""Registered synthetic fixture mode for @Step portal gating."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


MODE = register_mode(
    FixtureMode(
        name="step-portal",
        fixture_args=None,
        order=84,
        id_block=84,
        case=FixtureCase(
            name="step-portal",
            mode=None,
            tests=(TestCase("test_step_portal.py", (), True, True),),
            ports={"native": 2939, "asan": 2940},
            generator="make_step_portal_fixture.py",
            output="step-portal",
        ),
    )
)
