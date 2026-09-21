#!/usr/bin/env python3
"""Compile and run the small CRefPtr validity contract test."""

from __future__ import annotations

import os
import pathlib
import shutil
import subprocess
import sys
import tempfile


ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = pathlib.Path(__file__).with_suffix(".cpp")


def main() -> int:
    compiler = shutil.which(os.environ.get("CXX", "g++"))
    if compiler is None:
        print("CRefPtr test requires a C++ compiler", file=sys.stderr)
        return 2

    with tempfile.TemporaryDirectory(prefix="sphere99-refptr-test-") as temporary:
        binary = pathlib.Path(temporary) / "test_refptr"
        build = subprocess.run(
            [compiler, "-std=c++11", str(SOURCE), "-o", str(binary)],
            check=False,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        if build.returncode != 0:
            print("CRefPtr test did not compile", file=sys.stderr)
            return build.returncode

        test = subprocess.run([str(binary)], check=False)
        if test.returncode != 0:
            print(f"CRefPtr validity test failed at check {test.returncode}")
            return test.returncode

    print("CRefPtr validity test passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
