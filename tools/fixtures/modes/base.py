"""Registered synthetic fixture mode: base."""

from .registry import FixtureCase, FixtureMode, TestCase, register_mode

MODE = register_mode(
    FixtureMode(
        name='base',
        fixture_args=(),
        order=150,
        id_block=1,
    )
)
