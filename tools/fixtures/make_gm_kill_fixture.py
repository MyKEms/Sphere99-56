#!/usr/bin/env python3
"""Generate the targeted-GM-command fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.gm_kill import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_gm_kill_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
