"""Discoverable synthetic fixture mode registry."""

from __future__ import annotations

import importlib
import pkgutil
import sys

from .registry import FixtureMode, all_modes, reset_registry


def discover_modes() -> tuple[FixtureMode, ...]:
    """Import every mode module and validate its independent id range."""
    reset_registry()
    for module in sorted(pkgutil.iter_modules(__path__), key=lambda item: item.name):
        if module.name.startswith("_") or module.name == "registry":
            continue
        qualified_name = f"{__name__}.{module.name}"
        if qualified_name in sys.modules:
            importlib.reload(sys.modules[qualified_name])
        else:
            importlib.import_module(qualified_name)
    return all_modes()
