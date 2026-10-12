"""Registered fixture for ordinary-player self commands while dead."""

from __future__ import annotations

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


MODE = register_mode(
    FixtureMode(
        name="ghost-self-commands",
        fixture_args=None,
        order=186,
        id_block=143,
        case=FixtureCase(
            name="ghost-self-commands",
            mode=None,
            tests=(TestCase("test_ghost_self_commands.py", (), True, True),),
            ports={"native": 6012, "asan": 9012},
            generator="make_ghost_self_commands_fixture.py",
            output="ghost-self-commands",
        ),
    )
)
