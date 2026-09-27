"""Discoverable synthetic fixture mode registry."""

from __future__ import annotations

import importlib
import pkgutil
import sys
from collections.abc import Callable
from pathlib import Path

from .registry import FixtureMode, all_modes, reset_registry, validate_modes


_GENERATORS: dict[str, Callable[[Path], int]] = {}


def discover_modes() -> tuple[FixtureMode, ...]:
    """Import every mode module and validate its independent id range."""
    _GENERATORS.clear()
    reset_registry()
    for module in sorted(pkgutil.iter_modules(__path__), key=lambda item: item.name):
        if module.name.startswith("_") or module.name == "registry":
            continue
        qualified_name = f"{__name__}.{module.name}"
        if qualified_name in sys.modules:
            module_object = importlib.reload(sys.modules[qualified_name])
        else:
            module_object = importlib.import_module(qualified_name)
        mode = getattr(module_object, "MODE", None)
        if mode is None:
            continue
        generator = getattr(module_object, "generate", None)
        if mode.fixture_args is not None:
            if generator is None:
                raise ValueError(f"fixture mode {mode.name} has no generator")
            _GENERATORS[mode.name] = generator
    return all_modes()


def generator_for(name: str) -> Callable[[Path], int]:
    """Return the generator owned by an auto-discovered fixture mode."""

    try:
        return _GENERATORS[name]
    except KeyError as error:
        raise KeyError(f"fixture mode {name} has no generator") from error


def generators() -> dict[str, Callable[[Path], int]]:
    """Return a snapshot of the generators discovered with the mode modules."""

    return dict(_GENERATORS)
