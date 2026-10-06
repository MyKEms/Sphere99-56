"""Registered fixture for reserved memory-object type dispatch."""

from __future__ import annotations

from pathlib import Path

from .combat_scheduler import generate as generate_combat
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output: Path) -> int:
    """Generate the combat fixture with a conflicting script memory type.

    ITEMID_MEMORY is a built-in object kind.  The trailing definition models a
    malformed or shadowing script entry that must not change its C++ subtype.
    """

    result = generate_combat(output)
    tables = output / "scripts" / "spheretables.scp"
    tables.write_text(
        tables.read_text(encoding="ascii")
        + """

[ITEMDEF 0x2007]
DEFNAME=i_memory_shadow
TYPE=T_EQ_SCRIPT
LAYER=30
""",
        encoding="ascii",
    )
    return result


MODE = register_mode(
    FixtureMode(
        name="sector-tick-exception",
        fixture_args=(),
        order=172,
        id_block=119,
        case=FixtureCase(
            name="sector-tick-exception",
            mode="sector-tick-exception",
            tests=(
                TestCase("test_sector_tick_exception.py", (), True, True),
            ),
            ports={"native": 3000, "asan": 3001},
            mode_by_variant={},
            generator="make_fixture.py",
            generator_args=(),
            output="sector-tick-exception",
            test_args_by_variant={},
        ),
    )
)
