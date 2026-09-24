#!/usr/bin/env python3
"""Compile the 0.99 resource-ID type-gap regression fixture."""

from __future__ import annotations

import argparse
import subprocess
import tempfile
from pathlib import Path


PROBE_SOURCE = r'''
#include "SphereCommon/spherecommon.h"

static_assert(RES_Clients == 8, "Clients must retain the stock resource type");
static_assert(RES_Package == 27, "Package must retain the stock resource type");
static_assert(RES_GET_TYPE(0x90000123u) == RES_Clients,
    "the stock Clients literal must decode as Clients");
static_assert(RES_GET_TYPE(0xB6000456u) == RES_Package,
    "the stock Package literal must decode as Package");

int main() { return 0; }
'''


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", default="g++")
    args = parser.parse_args()
    repo = Path(__file__).resolve().parents[2]
    with tempfile.TemporaryDirectory(prefix="sphere-resource-types-") as temp:
        temp_path = Path(temp)
        source = temp_path / "resource_type_probe.cpp"
        object = temp_path / "resource_type_probe.o"
        source.write_text(PROBE_SOURCE, encoding="utf-8")
        compile_result = subprocess.run(
            [
                args.compiler,
                "-std=c++14",
                "-DSPHERE_SVR",
                "-I",
                str(repo),
                "-I",
                str(repo / "SphereCommon"),
                "-I",
                str(repo / "spherelib"),
                "-I",
                str(repo / "SphereSvr"),
                "-I",
                str(repo / "SphereAccount"),
                "-c",
                str(source),
                "-o",
                str(object),
            ],
            text=True,
            capture_output=True,
        )
        if compile_result.returncode:
            print(compile_result.stdout, end="")
            print(compile_result.stderr, end="")
            return compile_result.returncode
        print("Clients=0x90000123 type=8")
        print("Package=0xB6000456 type=27")
        return 0


if __name__ == "__main__":
    raise SystemExit(main())
