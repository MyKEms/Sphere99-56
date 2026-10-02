"""Registered fixture for inline TEXTA controls in both script spellings."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode


MODE = register_mode(
    FixtureMode(
        name="texta-inline",
        fixture_args=None,
        order=92,
        id_block=92,
        case=FixtureCase(
            name="texta-inline",
            mode=None,
            tests=(TestCase("test_texta_inline.py", (), True, True),),
            ports={"native": 2992, "asan": 2993},
            mode_by_variant={},
            generator="make_texta_fixture.py",
            generator_args=(),
            output="texta-inline",
            test_args_by_variant={},
        ),
    )
)
