#!/usr/bin/env python3
"""Generate the standalone definition-root/STRFIRSTCAP fixture."""

from __future__ import annotations

import sys
from pathlib import Path

from modes.definition_root import generate


if __name__ == "__main__":
    if len(sys.argv) != 2:
        raise SystemExit("usage: make_definition_root_fixture.py OUTPUT")
    raise SystemExit(generate(Path(sys.argv[1])))
