#!/usr/bin/env python3
"""Generate the resource-UID character fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.resource_char_uid import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_resource_char_uid_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
