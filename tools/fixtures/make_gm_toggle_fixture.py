#!/usr/bin/env python3
"""Generate the standalone GM-mode command fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.gm_toggle import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_gm_toggle_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
