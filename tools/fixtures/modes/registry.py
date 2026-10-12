"""Shared registration API for isolated synthetic fixture modes."""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Mapping, Sequence


ID_BLOCK_SIZE = 0x1000
ID_BLOCK_BASE = 0x00500000


@dataclass(frozen=True)
class TestCase:
    script: str
    args: tuple[str, ...] = ()
    pass_fixture: bool = True
    pass_port: bool = True


@dataclass(frozen=True)
class FixtureCase:
    name: str
    mode: str | None
    tests: tuple[TestCase, ...]
    ports: Mapping[str, int] = field(default_factory=dict)
    mode_by_variant: Mapping[str, str] = field(default_factory=dict)
    generator: str = "make_fixture.py"
    generator_args: tuple[str, ...] = ()
    output: str | None = None
    test_args_by_variant: Mapping[str, tuple[str, ...]] = field(default_factory=dict)

    def selected_mode(self, variant: str) -> str | None:
        return self.mode_by_variant.get(variant, self.mode)

    def selected_port(self, variant: str) -> int | None:
        return self.ports.get(variant)

    def selected_test_args(self, variant: str) -> tuple[str, ...]:
        return self.test_args_by_variant.get(variant, ())


@dataclass(frozen=True)
class FixtureMode:
    name: str
    fixture_args: tuple[str, ...] | None = None
    order: int = 0
    id_block: int = 0
    case: FixtureCase | None = None

    @property
    def id_start(self) -> int:
        return ID_BLOCK_BASE + self.id_block * ID_BLOCK_SIZE

    @property
    def id_end(self) -> int:
        return self.id_start + ID_BLOCK_SIZE - 1

    def allocate_ids(self, count: int, *, offset: int = 0) -> tuple[int, ...]:
        """Allocate deterministic synthetic ids inside this mode's private block."""
        if count < 0 or offset < 0 or offset + count > ID_BLOCK_SIZE:
            raise ValueError(f"{self.name} requested ids outside its private range")
        return tuple(self.id_start + offset + index for index in range(count))


_MODES: dict[str, FixtureMode] = {}


def register_mode(mode: FixtureMode) -> FixtureMode:
    if mode.name in _MODES:
        raise ValueError(f"duplicate fixture mode: {mode.name}")
    if mode.id_block < 0:
        raise ValueError(f"fixture mode {mode.name} has a negative id block")
    _MODES[mode.name] = mode
    return mode


def reset_registry() -> None:
    _MODES.clear()


def all_modes() -> tuple[FixtureMode, ...]:
    modes = tuple(sorted(_MODES.values(), key=lambda item: (item.order, item.name)))
    validate_modes(modes)
    return modes


def validate_modes(modes: Sequence[FixtureMode]) -> None:
    """Reject any overlapping synthetic ID ranges.

    The registry allocates fixed-size blocks today, but validating the actual
    interval keeps the guard correct if a mode later requests a different
    block size or a custom allocator.
    """

    by_range = sorted(modes, key=lambda item: (item.id_start, item.id_end, item.name))
    for left, right in zip(by_range, by_range[1:]):
        if left.id_end >= right.id_start:
            raise ValueError(
                f"fixture modes {left.name} ({left.id_start:#x}-{left.id_end:#x}) "
                f"and {right.name} ({right.id_start:#x}-{right.id_end:#x}) "
                "have overlapping synthetic id ranges"
            )


def validate_ports(cases: Mapping[str, FixtureCase]) -> tuple[str, ...]:
    """Return listen/game endpoint collisions between fixture cases.

    Native and ASan cases run in separate jobs, so each variant has its own
    endpoint namespace.  A case owns both its listen port and the game port
    at ``listen + 1000``; either endpoint colliding with another case is a
    manifest error because a leaked server can make the following case fail.
    """

    errors: list[str] = []
    for variant in ("native", "asan"):
        endpoints: dict[int, tuple[str, str]] = {}
        for name, case in sorted(cases.items()):
            port = case.selected_port(variant)
            if port is None:
                continue
            for endpoint, role in ((port, "listen"), (port + 1000, "game")):
                previous = endpoints.get(endpoint)
                if previous is not None:
                    previous_name, previous_role = previous
                    errors.append(
                        f"{variant} port {endpoint} is used by "
                        f"{previous_name} ({previous_role}) and {name} ({role})"
                    )
                else:
                    endpoints[endpoint] = (name, role)
    return tuple(errors)


def make_case(
    name: str,
    *,
    mode: str | None,
    script: str,
    port: tuple[int, int] | None = None,
    args: Sequence[str] = (),
    output: str | None = None,
    generator: str = "make_fixture.py",
    generator_args: Sequence[str] = (),
    mode_by_variant: Mapping[str, str] | None = None,
    tests: Sequence[TestCase] | None = None,
    test_args_by_variant: Mapping[str, Sequence[str]] | None = None,
    pass_fixture: bool = True,
    pass_port: bool = True,
) -> FixtureCase:
    selected_tests = tuple(
        tests
        or (TestCase(script, tuple(args), pass_fixture, pass_port),)
    )
    selected_ports = {} if port is None else {"native": port[0], "asan": port[1]}
    selected_test_args = {
        variant: tuple(values)
        for variant, values in (test_args_by_variant or {}).items()
    }
    return FixtureCase(
        name=name,
        mode=mode,
        tests=selected_tests,
        ports=selected_ports,
        mode_by_variant=dict(mode_by_variant or {}),
        generator=generator,
        generator_args=tuple(generator_args),
        output=output,
        test_args_by_variant=selected_test_args,
    )
