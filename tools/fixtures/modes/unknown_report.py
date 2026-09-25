"""Registered synthetic fixture mode: unknown-report."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='unknown-report',
        fixture_args=('--unknown-keyword-report',),
        order=151,
        id_block=2,
    )
)
