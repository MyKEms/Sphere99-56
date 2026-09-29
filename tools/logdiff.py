#!/usr/bin/env python3
"""Normalize and compare Sphere daily logs without exposing run-specific data."""

from __future__ import annotations

import argparse
import collections
import json
import re
from pathlib import Path
from typing import Iterable, Mapping, Sequence


_LEVELS = {
    "TRACE": "DEBUG",
    "DEBUG": "DEBUG",
    "EVENT": "INFO",
    "INFO": "INFO",
    "WARN": "WARNING",
    "WARNING": "WARNING",
    "ERROR": "ERROR",
    "CRIT": "CRITICAL",
    "CRITICAL": "CRITICAL",
    "FATAL": "CRITICAL",
}
_LEVEL_RE = re.compile(
    r"^(?P<level>DEBUG|TRACE|EVENT|INFO|WARN|WARNING|ERROR|CRIT|CRITICAL|FATAL):"
)
_TIME_RE = re.compile(r"^\d{2}:\d{2}(?::\d{2})?:")
_BRACKET_LEVEL_RE = re.compile(r"^\[(?P<level>DEBUG|TRACE|INFO|WARN|WARNING|ERROR|CRIT|CRITICAL|FATAL)\]\s*")
_CONTEXT_RE = re.compile(r"^\((?:[^()\r\n]+),\s*(\d+)\)")
_CLIENT_SOCKET_RE = re.compile(r"^[0-9A-Fa-f]+:")
_IP_RE = re.compile(r"\b(?:\d{1,3}\.){3}\d{1,3}\b")
_HEX_RE = re.compile(r"(?<![\w])(?:0x[0-9a-fA-F]+|0[0-9a-fA-F]{3,})(?![\w])")
_NUMBER_RE = re.compile(r"(?<![\w])[-+]?\d+(?:\.\d+)?(?![\w])")
_QUOTED_RE = re.compile(r"(['\"])(?:\\.|(?!\1).)*\1")
_NAMED_VALUE_RE = re.compile(
    r"\b(?P<prefix>(?:acct|account|char|character|name|user|ip)\s*=\s*)(?P<value>[A-Za-z0-9_.@-]+)",
    re.IGNORECASE,
)


def _level_and_body(line: str) -> tuple[str, str]:
    body = line.strip()
    body = _TIME_RE.sub("", body, count=1)
    bracket = _BRACKET_LEVEL_RE.match(body)
    if bracket:
        return _LEVELS[bracket.group("level")], body[bracket.end() :]
    level = _LEVEL_RE.match(body)
    if level:
        return _LEVELS[level.group("level")], body[level.end() :]
    return "INFO", body


def normalize_line(line: str) -> str:
    """Return a stable level/context/template representation of one log line."""

    level, body = _level_and_body(line)
    context = _CONTEXT_RE.match(body)
    prefix = ""
    if context:
        prefix = f"(script,{context.group(1)})"
        body = body[context.end() :]

    # Client records carry the socket handle as a hexadecimal prefix.  The
    # handle is allocated afresh for every connection and is not part of the
    # stock event class being compared.
    body = _CLIENT_SOCKET_RE.sub("", body, count=1)

    # Replace identifying values before numbers so addresses and quoted names
    # cannot leak into a diff report.
    body = _IP_RE.sub("<ip>", body)
    body = _QUOTED_RE.sub(lambda match: match.group(1) + "<value>" + match.group(1), body)
    body = _NAMED_VALUE_RE.sub(lambda match: match.group("prefix") + "<value>", body)
    body = _HEX_RE.sub("<hex>", body)
    body = _NUMBER_RE.sub("<n>", body)
    body = re.sub(r"\s+", " ", body).strip()
    return f"{level}:{prefix}{body}"


def _counts(lines: Iterable[str]) -> collections.Counter[str]:
    return collections.Counter(
        normalized
        for line in lines
        if (normalized := normalize_line(line))
    )


def diff_logs(linux_lines: Sequence[str], windows_lines: Sequence[str]) -> dict[str, object]:
    """Compare normalized line classes and return JSON-friendly aggregates."""

    linux = _counts(linux_lines)
    windows = _counts(windows_lines)
    only_linux = {key: linux[key] for key in sorted(linux.keys() - windows.keys())}
    only_windows = {key: windows[key] for key in sorted(windows.keys() - linux.keys())}
    ratios: dict[str, dict[str, float | int]] = {}
    for key in sorted(linux.keys() & windows.keys()):
        left = linux[key]
        right = windows[key]
        ratios[key] = {
            "linux": left,
            "windows": right,
            "ratio": round(left / right, 3) if right else None,
        }
    return {
        "only_linux": only_linux,
        "only_windows": only_windows,
        "ratios": ratios,
    }


def _read_lines(path: Path) -> list[str]:
    return path.read_text(encoding="utf-8", errors="replace").splitlines()


def _print_report(report: Mapping[str, object]) -> None:
    for label in ("only_linux", "only_windows"):
        print(f"classes only in {label.removeprefix('only_')}:")
        values = report[label]
        if not values:
            print("  (none)")
        else:
            for key, count in values.items():
                print(f"  {count:>6}  {key}")
    print("count ratios:")
    ratios = report["ratios"]
    if not ratios:
        print("  (none)")
    else:
        for key, value in ratios.items():
            ratio = value["ratio"]
            ratio_text = "n/a" if ratio is None else f"{ratio:.3f}"
            print(f"  {value['linux']:>6}/{value['windows']:<6} {ratio_text}  {key}")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("linux_log", type=Path)
    parser.add_argument("windows_log", type=Path)
    parser.add_argument("--json", action="store_true", dest="as_json")
    args = parser.parse_args()
    report = diff_logs(_read_lines(args.linux_log), _read_lines(args.windows_log))
    if args.as_json:
        print(json.dumps(report, indent=2, sort_keys=True))
    else:
        _print_report(report)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
