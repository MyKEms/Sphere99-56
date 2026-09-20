#!/usr/bin/env python3
"""Create a synthetic runtime with exactly five coverage-tracked sections."""

from __future__ import annotations

import argparse
from pathlib import Path

from make_fixture import skill_sections, write_mul_fixture, write_runtime_files, write_text


def write_coverage_scripts(root: Path, on_demand_report: bool) -> None:
    scripts = root / "scripts"
    scripts.mkdir(parents=True, exist_ok=True)
    report_command = "SERV.SCRIPTCOVERAGEREPORT\n" if on_demand_report else ""
    write_text(
        scripts / "spheretables.scp",
        f"""; Synthetic script coverage fixture.

[TYPEDEF 0]
DEFNAME=T_NORMAL

[TYPEDEF 1]
DEFNAME=CONTAINER

[TYPEDEF 61]
DEFNAME=T_HAIR

[ITEMDEF 0x0E75]
DEFNAME=DEFAULTITEM
NAME=synthetic container
TYPE=CONTAINER

[ITEMDEF 0x0E72]
DEFNAME=SYNTHETIC_MAGERY_START
NAME=synthetic magery token
TYPE=T_NORMAL

[ITEMDEF 0x0E73]
DEFNAME=SYNTHETIC_RESIST_START
NAME=synthetic resist token
TYPE=T_NORMAL

[ITEMDEF 0x0203B]
DEFNAME=SYNTHETIC_HAIR
NAME=synthetic hair
TYPE=T_HAIR

[ITEMDEF 0x0205A]
DEFNAME=SYNTHETIC_DEFAULT_HAIR
NAME=synthetic default hair
TYPE=T_HAIR

[ITEMDEF 0x09B2]
DEFNAME=SYNTHETIC_SHIRT
NAME=synthetic shirt
TYPE=T_NORMAL

[CHARDEF 0x0190]
DEFNAME=c_MAN
DEFNAME2=DEFAULTCHAR
NAME=synthetic human
ID=0x0190
STR=100
DEX=100
ARMOR=5,5

[CHARDEF 0x0191]
DEFNAME=c_WOMAN
NAME=synthetic human
ID=0x0191
STR=100
DEX=100

[FUNCTION f_coverage_alpha]
RETURN 11

[FUNCTION f_coverage_beta]
RETURN 22

[FUNCTION f_coverage_never]
RETURN 33

[EVENTS e_AllPlayers]
ON=@LogIn
VAR coverage_alpha,<f_coverage_alpha>
VAR coverage_beta,<f_coverage_beta>
{report_command}ON=@CoverageNever
RETURN 0

[SPEECH spk_AllPlayers]

[AREA Synthetic world]
P=128,128,0
RECT=1,1,6143,4096
""" + skill_sections() + """

[NEWBIE MAGERY]
ITEMNEWBIE=0x0E72

[NEWBIE RESIST]
ITEMNEWBIE=0x0E73
""",
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="external directory to populate")
    parser.add_argument(
        "--disabled",
        action="store_true",
        help="omit the opt-in report setting to exercise default-off behavior",
    )
    parser.add_argument(
        "--on-demand-report",
        action="store_true",
        help="request a report through SERV.SCRIPTCOVERAGEREPORT during login",
    )
    args = parser.parse_args()

    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")

    write_runtime_files(root)
    config = root / "sphere.ini"
    text = config.read_text(encoding="ascii")
    marker = "SECURE=1\n"
    if marker not in text:
        parser.error("fixture configuration is missing its SPHERE settings")
    report_setting = (
        "" if args.disabled else
        "SCRIPTEXECUTIONREPORT=logs/script-execution-coverage.json\n"
    )
    config.write_text(
        text.replace(marker, marker + report_setting, 1),
        encoding="ascii",
    )
    write_coverage_scripts(root, args.on_demand_report)
    write_mul_fixture(root, extra_item_id=0x205A)
    print(f"wrote synthetic script-coverage fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
