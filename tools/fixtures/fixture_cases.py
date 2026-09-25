"""Compatibility exports for the discoverable fixture-mode manifest."""

from __future__ import annotations

from modes import discover_modes
from modes.registry import FixtureCase, FixtureMode, TestCase

MODES = discover_modes()
MODE_REGISTRY = {mode.name: mode for mode in MODES}
FIXTURE_MODES = {
    mode.name: mode.fixture_args
    for mode in MODES
    if mode.fixture_args is not None
}
FIXTURE_CASES = {
    mode.name: mode.case
    for mode in MODES
    if mode.case is not None
}
NATIVE_CASES = tuple(FIXTURE_CASES)
ASAN_CASES = tuple(FIXTURE_CASES)

__all__ = [
    "ASAN_CASES",
    "FIXTURE_CASES",
    "FIXTURE_MODES",
    "FixtureCase",
    "FixtureMode",
    "MODE_REGISTRY",
    "MODES",
    "NATIVE_CASES",
    "TestCase",
]
