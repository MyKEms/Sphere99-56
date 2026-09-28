"""Registered synthetic fixture mode: deferred account teardown."""

from __future__ import annotations

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT_NAME = "AccountLifetimeProbe"
ACCOUNT_PASSWORD = "acct-life-pw"


def generate(output: Path) -> int:
    """Generate the common fixture, then seed a temporary account."""

    result = generate_recipe(output, MODE)
    if result:
        return result
    (output / "accounts" / "sphereaccu.scp").write_text(
        "\n".join(
            (
                f"[{ACCOUNT_NAME}]",
                f"PASSWORD={ACCOUNT_PASSWORD}",
                "PRIV=0x8000",
                "[EOF]",
                "",
            )
        ),
        encoding="ascii",
    )
    return 0


MODE = register_mode(
    FixtureMode(
        name="account-lifetime",
        fixture_args=(),
        order=63,
        id_block=67,
        case=FixtureCase(
            name="account-lifetime",
            mode="account-lifetime",
            tests=(TestCase("test_account_lifetime.py", (), True, True),),
            ports={"native": 2866, "asan": 2866},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="account-lifetime",
            test_args_by_variant={},
        ),
    )
)
