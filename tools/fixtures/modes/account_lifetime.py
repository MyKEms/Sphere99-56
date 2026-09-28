"""Registered synthetic fixture mode: deferred account teardown."""

from __future__ import annotations

from pathlib import Path

from .legacy_generator import generate as generate_recipe
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


ACCOUNT_NAME = "AccountLifetimeProbe"
ACCOUNT_PASSWORD = "acct-life-pw"
LINGER_ACCOUNT_NAME = "AccountLingerProbe"
LINGER_ACCOUNT_PASSWORD = "acct-linger-pw"


def generate(output: Path) -> int:
    """Generate the common fixture, then seed a temporary account."""

    result = generate_recipe(output, MODE)
    if result:
        return result
    scripts_path = output / "scripts" / "spheretables.scp"
    scripts = scripts_path.read_text(encoding="ascii")
    logout_marker = "ON=@Logout\nRETURN 0\n\n[CHARDEF 0x0191]"
    if scripts.count(logout_marker) != 1:
        raise RuntimeError("account lifetime fixture could not set the linger logout trigger")
    scripts_path.write_text(
        scripts.replace(logout_marker, "ON=@Logout\nRETURN 1\n\n[CHARDEF 0x0191]"),
        encoding="ascii",
    )
    (output / "accounts" / "sphereaccu.scp").write_text(
        "\n".join(
            (
                f"[{ACCOUNT_NAME}]",
                f"PASSWORD={ACCOUNT_PASSWORD}",
                "PRIV=0x8000",
                f"[{LINGER_ACCOUNT_NAME}]",
                f"PASSWORD={LINGER_ACCOUNT_PASSWORD}",
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
