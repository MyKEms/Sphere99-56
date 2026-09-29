#!/usr/bin/env python3
"""Generate the standalone legacy NEWEQUIP fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.newequip import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_newequip_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
