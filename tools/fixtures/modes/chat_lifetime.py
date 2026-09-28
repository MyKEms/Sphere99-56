"""Registered synthetic fixture mode: deferred chat-channel teardown."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output):
    """Generate this mode through the shared compatibility primitives."""

    from .legacy_generator import generate as generate_recipe

    return generate_recipe(output, MODE)


MODE = register_mode(
    FixtureMode(
        name="chat-lifetime",
        fixture_args=(),
        order=66,
        id_block=66,
        case=FixtureCase(
            name="chat-lifetime",
            mode="chat-lifetime",
            tests=(TestCase("test_chat_lifetime.py", (), True, True),),
            ports={"native": 2865, "asan": 2865},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="chat-lifetime",
            test_args_by_variant={},
        ),
    )
)
