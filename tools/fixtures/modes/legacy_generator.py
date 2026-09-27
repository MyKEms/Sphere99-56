"""Compatibility adapter for mode-owned fixture recipes.

The synthetic files still use the mature compatibility writer in
``make_fixture.py``.  A mode owns the recipe that selects that writer, while
this module keeps the import lazy so discovery never imports the command-line
entry point recursively.
"""

from __future__ import annotations

from pathlib import Path

from .registry import FixtureMode


def generate(output: Path, mode: FixtureMode) -> int:
    """Run *mode*'s declarative recipe through the compatibility writer."""

    from make_fixture import generate_registered_mode

    return generate_registered_mode(output, mode)
