"""Registered fixture for a callback-cancelled combat stroke."""

from __future__ import annotations

from pathlib import Path

from .combat_scheduler import ACCOUNT, PASSWORD, generate as generate_combat
from .registry import FixtureCase, FixtureMode, TestCase, register_mode


def generate(output: Path) -> int:
    result = generate_combat(output)
    accounts = output / "accounts" / "sphereaccu.scp"
    contents = accounts.read_text(encoding="ascii")
    contents = contents.replace(
        f"PASSWORD={PASSWORD}\n", f"PASSWORD={PASSWORD}\nPLEVEL=Admin\n", 1
    )
    accounts.write_text(contents, encoding="ascii")

    # The sector assertion is reached when a synthetic combat callback changes the
    # active action while Skill_Stage still dispatches the captured combat
    # skill.  Leave the callback's return at the default so the native
    # combat stage runs with the now-invalid active action.
    tables = output / "scripts" / "spheretables.scp"
    text = tables.read_text(encoding="ascii")
    insertion = "ON=@Stroke\nACTION=NONE\n"
    for marker in ("[SKILL 42]\n", "[SKILL 43]\n"):
        if marker not in text:
            raise RuntimeError(f"synthetic combat skill section missing: {marker.strip()}")
        text = text.replace(marker, marker + insertion, 1)
    tables.write_text(text, encoding="ascii")
    return result


MODE = register_mode(
    FixtureMode(
        name="fight-stale-state",
        fixture_args=(),
        order=192,
        id_block=144,
        case=FixtureCase(
            name="fight-stale-state",
            mode="fight-stale-state",
            tests=(TestCase("test_fight_stale_state.py", (), True, True),),
            ports={"native": 3002, "asan": 3003},
            output="fight-stale-state",
        ),
    )
)
