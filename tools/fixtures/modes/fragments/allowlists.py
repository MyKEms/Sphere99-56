"""Merge mode-owned checker allowlist fragments."""

from __future__ import annotations

import json
from pathlib import Path


_ROOT = Path(__file__).resolve().parent / "allowlists"


def unknown_keyword_allowlist() -> dict[str, object]:
    """Return the deterministic union of unknown-keyword fragments."""

    entries: list[dict[str, object]] = []
    for fragment in sorted((_ROOT / "unknown-keyword").glob("*.json")):
        data = json.loads(fragment.read_text(encoding="utf-8"))
        if data.get("version") not in (None, 1):
            raise ValueError(f"unsupported allowlist fragment version: {fragment}")
        fragment_entries = data.get("entries")
        if not isinstance(fragment_entries, list):
            raise ValueError(f"allowlist fragment has no entries list: {fragment}")
        entries.extend(fragment_entries)
    return {"version": 1, "entries": entries}


def write_unknown_keyword_allowlist(path: Path) -> None:
    """Write the merged checker allowlist to a disposable path."""

    path.write_text(
        json.dumps(unknown_keyword_allowlist(), indent=2) + "\n",
        encoding="utf-8",
    )
