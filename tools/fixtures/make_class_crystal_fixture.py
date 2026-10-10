#!/usr/bin/env python3
"""Generate the standalone protected class-crystal fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.w16_class_crystal import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_class_crystal_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
