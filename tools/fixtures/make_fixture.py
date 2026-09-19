#!/usr/bin/env python3
"""Create a small, redistributable runtime fixture for protocol tests.

The generated files are synthetic and are written to the caller-provided
directory.  Nothing from a client installation, shard runtime, world save, or
account database is read or copied.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


MAP_BLOCK_BYTES = 196
MAP_BLOCKS_X = 0x1800 // 8
MAP_BLOCKS_Y = 0x1000 // 8
TERRAIN_QTY = 0x4000
TILE_BLOCK_QTY = 32
TERRAIN_RECORD_BYTES = 26
ITEM_RECORD_BYTES = 37
DEFAULT_ITEM_ID = 0x0E75


def write_sparse(path: Path, size: int) -> None:
    """Create a zero-filled sparse file of exactly *size* bytes."""

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as stream:
        if size:
            stream.seek(size - 1)
            stream.write(b"\0")


def write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(data)


def write_text(path: Path, text: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text.rstrip() + "\n", encoding="ascii")


def terrain_size() -> int:
    header_bytes = (TERRAIN_QTY // TILE_BLOCK_QTY) * 4
    return header_bytes + TERRAIN_QTY * TERRAIN_RECORD_BYTES


def tiledata_size(max_item_id: int) -> int:
    item_header_bytes = ((max_item_id // TILE_BLOCK_QTY) + 1) * 4
    item_bytes = (max_item_id + 1) * ITEM_RECORD_BYTES
    return terrain_size() + item_header_bytes + item_bytes


def write_container_tile(path: Path, item_id: int) -> None:
    """Mark one synthetic item as a container in the tiledata layout."""

    record_offset = (
        terrain_size()
        + ((item_id // TILE_BLOCK_QTY) * 4)
        + 4
        + (item_id * ITEM_RECORD_BYTES)
    )
    # UFLAG3_CONTAINER, movable weight, no layer/animation, one tile high.
    record = struct.pack(
        "<IBBIIHB20s",
        0x00200000,
        1,
        0,
        0,
        0,
        0,
        1,
        b"synthetic container\0".ljust(20, b"\0"),
    )
    with path.open("r+b") as stream:
        stream.seek(record_offset)
        stream.write(record)


def write_mul_fixture(root: Path) -> None:
    muls = root / "muls"
    map_blocks = MAP_BLOCKS_X * MAP_BLOCKS_Y
    write_sparse(muls / "map0.mul", map_blocks * MAP_BLOCK_BYTES)
    write_sparse(muls / "staidx0.mul", map_blocks * 12)
    write_bytes(muls / "statics0.mul", b"")
    write_bytes(muls / "multi.idx", b"\0" * 12)
    write_bytes(muls / "multi.mul", b"")

    # The server only needs the file layout for the synthetic item definition;
    # all terrain and other item records remain zero/default data.
    tiledata = muls / "tiledata.mul"
    write_sparse(tiledata, tiledata_size(DEFAULT_ITEM_ID))
    write_container_tile(tiledata, DEFAULT_ITEM_ID)

    # Hues are not opened by the server startup mask, but keeping a tiny valid
    # placeholder makes the generated MUL directory explicit and complete for
    # tools that inspect the fixture.
    write_bytes(muls / "hues.mul", b"\0\0\0\0")


def skill_sections() -> str:
    sections = []
    for skill_id in range(50):
        skill_key = {
            25: "MAGERY",
            26: "RESIST",
        }.get(skill_id, f"SYNTH_SKILL_{skill_id}")
        sections.append(
            f"[SKILL {skill_id}]\n"
            f"KEY={skill_key}\n"
            "TITLE=synthetic skill\n"
            "STAT_STR=33\n"
            "STAT_INT=34\n"
            "STAT_DEX=33\n"
            "BONUS_STATS=0\n"
            "BONUS_STR=0\n"
            "BONUS_INT=0\n"
            "BONUS_DEX=0\n"
            "DELAY=1\n"
            "EFFECT=0\n"
            "ADV_RATE=0,0,0\n"
        )
    return "\n".join(sections)


def write_scripts(
    root: Path,
    *,
    unknown_newbie: bool = False,
    unknown_keyword_probe: bool = False,
    unknown_keyword_set_probe: bool = False,
    unknown_keyword_normalization_probe: bool = False,
    unknown_keyword_overflow_probe: bool = False,
    unknown_keyword_admin_probe: bool = False,
    unknown_keyword_rejected_probe: bool = False,
) -> None:
    unknown_newbie_section = (
        "\n[NEWBIE SYNTHETIC_UNKNOWN_SKILL]\nITEMNEWBIE=0x0E72\n"
        if unknown_newbie
        else ""
    )
    unknown_keyword_probe_lines = []
    if (
        unknown_keyword_probe
        or unknown_keyword_set_probe
        or unknown_keyword_normalization_probe
        or unknown_keyword_admin_probe
    ):
        unknown_keyword_probe_lines.extend(
            [
                "UNKNOWN_REPORT_PROBE_COMMAND",
                "SYSMESSAGE SPHERE_UNKNOWN_PROBE_PROPERTY <UNKNOWN_REPORT_PROBE_PROPERTY>",
                "SYSMESSAGE SPHERE_UNKNOWN_PROBE_FUNCTION <UNKNOWN_REPORT_PROBE_FUNCTION()>",
                "TRIGGER @UnknownReportProbe",
            ]
        )
    if unknown_keyword_normalization_probe:
        unknown_keyword_probe_lines.extend(
            [
                "SYSMESSAGE SPHERE_UNKNOWN_NORMALIZED_DOTTED <UNKNOWN_REPORT_DOTTED_TAG.name>",
                "SYSMESSAGE SPHERE_UNKNOWN_NORMALIZED_INDEX <UNKNOWN_REPORT_INDEX_ARGV[3]>",
            ]
        )
    if unknown_keyword_set_probe:
        unknown_keyword_probe_lines.append("UNKNOWN_REPORT_PROBE_SET=1")
    if unknown_keyword_admin_probe:
        unknown_keyword_probe_lines.append("SERV.UNKNOWNREPORT")
    if unknown_keyword_rejected_probe:
        unknown_keyword_probe_lines.extend(
            [
                "ARG(unknown_report_bad_qty,one,two)",
                "ARG(,unknown_report_bad_arguments)",
            ]
        )
    if unknown_keyword_probe_lines:
        unknown_keyword_probe_lines.append("SYSMESSAGE SPHERE_UNKNOWN_PROBE_KNOWN <EVAL 1+2>")
    unknown_keyword_probe_script = "\n".join(unknown_keyword_probe_lines)
    if unknown_keyword_probe_script:
        unknown_keyword_probe_script += "\n"
    unknown_keyword_overflow_script = (
        "".join(
            "SYSMESSAGE SPHERE_UNKNOWN_OVERFLOW "
            f"<UNKNOWN_REPORT_OVERFLOW_{index:04d}>\n"
            for index in range(1025)
        )
        if unknown_keyword_overflow_probe
        else ""
    )
    write_text(
        root / "scripts" / "spheretables.scp",
        """; Synthetic definitions generated by tools/fixtures/make_fixture.py.
; This file contains no client or shard data.

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
NAME=magery starting token
TYPE=T_NORMAL

[ITEMDEF 0x0E73]
DEFNAME=SYNTHETIC_RESIST_START
NAME=resist starting token
TYPE=T_NORMAL

[ITEMDEF 0x0203B]
DEFNAME=SYNTHETIC_HAIR
NAME=synthetic hair
TYPE=T_HAIR
ON=@Create
TRIGGER @FixtureItemCustom, 7, fixture-item, <SRC.SERIAL>
ON=@FixtureItemCustom
SRC.SYSMESSAGE SPHERE_ITEM_TRIGGER <SRC.NAME>|<ARGN>|<ARGS>|<ARGO.NAME>
RETURN 1

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
ON=@FixtureTypeCustom
SYSMESSAGE SPHERE_CHARDEF_TRIGGER <SRC.NAME>|<ARGN>|<ARGS>|<ARGO.NAME>
RETURN 1

[CHARDEF 0x0191]
DEFNAME=c_WOMAN
NAME=synthetic human
ID=0x0191
STR=100
DEX=100

[EVENTS e_AllPlayers]
ON=@LogIn
ARG(trigger_value,5)
SYSMESSAGE SPHERE_ARG_SET <ARG(value,30)>|<ARG.value>|<ARG.trigger_value>
SYSMESSAGE SPHERE_ARG_MACRO <?ARG(macro_value,31)?>|<ARG.macro_value>
SYSMESSAGE SPHERE_ARG_FUNCTION <f_arg_local_outer>|<ARG.trigger_value>
SYSMESSAGE SPHERE_TABLE_SMOKE <EVAL 1+2>|<STRLEN abc>|<RAND 1>|<ISNUM 123>|<STRCMP abc,abc>
SYSMESSAGE SPHERE_NEWBIE_MAGERY <RESCOUNT(0x0E72)>
SYSMESSAGE SPHERE_NEWBIE_RESIST <RESCOUNT(0x0E73)>
TRIGGER @FixtureCustom, 42, fixture-char, <SRC.SERIAL>
TRIGGER @FixtureTypeCustom, 21, fixture-typedef, <SRC.SERIAL>
SYSMESSAGE SPHERE_TRIGGER_RETURN <TRIGGER(@FixtureReturn)>
HITS=100
DAMAGE 10,2
SYSMESSAGE SPHERE_RANGE_ARMOR <HITS>
NEWITEM SYNTHETIC_HAIR
""" + unknown_keyword_probe_script + unknown_keyword_overflow_script + """
ON=@EnvironChange
RETURN
ON=@Logout
RETURN 0
ON=@FixtureCustom
SYSMESSAGE SPHERE_CHAR_TRIGGER <SRC.NAME>|<ARGN>|<ARGS>|<ARGO.NAME>
RETURN 1
ON=@FixtureReturn
RETURN 73

[FUNCTION f_arg_local_inner]
ARG(value,20)
ARG(child_only,99)
RETURN <ARG.value>

[FUNCTION f_arg_local_outer]
ARG(value,10)
ARG(text,alpha)
ARG(inner_value,<f_arg_local_inner>)
ARG(if_value,0)
IF (<ARG.value> == 10)
ARG(if_value,1)
ENDIF
ARG(while_value,0)
WHILE (<ARG.while_value> < 1)
ARG(while_value,1)
ENDWHILE
SYSMESSAGE SPHERE_ARG_NESTED <ARG.inner_value>|<ARG.value>|<ARG.child_only>END
SYSMESSAGE SPHERE_ARG_FLOW <ARG.if_value>|<ARG.while_value>
SYSMESSAGE SPHERE_ARG_LENGTH <?STRLEN(<ARG.text>)?>
SYSMESSAGE SPHERE_ARG_DEFAULT [<ARG.unset_value>]
RETURN 10

[SPEECH spk_AllPlayers]

[AREA Synthetic world]
P=128,128,0
RECT=1,1,6143,4096

""" + skill_sections() + """

[NEWBIE MAGERY]
ITEMNEWBIE=0x0E72

[NEWBIE resist]
ITEMNEWBIE=0x0E73
""" + unknown_newbie_section,
    )


def write_runtime_files(
    root: Path,
    *,
    unknown_keyword_report: bool = False,
    unknown_keyword_report_format: str = "json",
) -> None:
    unknown_keyword_report_setting = (
        f"UNKNOWNKEYWORDREPORT=logs/unknown-keywords.{unknown_keyword_report_format}\n"
        if unknown_keyword_report
        else ""
    )
    write_text(
        root / "sphere.ini",
        """; Disposable synthetic runtime configuration.
; Generated by tools/fixtures/make_fixture.py; not a shard configuration.

[SPHERE]
SERVNAME=Sphere99 synthetic fixture
ACCAPPS=Free
MULFILES=muls/
SCPFILES=scripts/
RESOURCES=spheretables.scp
WORLDSAVE=save/
ACCTFILES=accounts/
LOG=logs/
DEBUGLEVEL=0
CLIENTMAX=64
CLIENTSPERIP=64
MAXCHARS=5
SAVEPERIOD=1440
SAVEBACKGROUND=0
CLIENTLINGER=60
SECURE=1
""" + unknown_keyword_report_setting + """

[STARTS]
Synthetic land
Synthetic starting point
128,128,0
""",
    )
    write_text(root / "save" / "sphereworld.scp", "[EOF]")
    write_text(root / "save" / "spherechars.scp", "[EOF]")
    write_text(root / "accounts" / "sphereaccu.scp", "[EOF]")
    (root / "logs").mkdir(parents=True, exist_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="directory to populate")
    parser.add_argument(
        "--unknown-newbie",
        action="store_true",
        help="include a section keyed by a nonexistent skill",
    )
    parser.add_argument(
        "--unknown-keyword-report",
        action="store_true",
        help="enable the configured runtime unknown-keyword report",
    )
    parser.add_argument(
        "--unknown-keyword-report-format",
        choices=("json", "csv"),
        default="json",
        help="select the report extension and output format",
    )
    parser.add_argument(
        "--unknown-keyword-probe",
        action="store_true",
        help="include one login hook with four intentional unknown keywords",
    )
    parser.add_argument(
        "--unknown-keyword-normalization-probe",
        action="store_true",
        help="include dotted-property and numeric-index normalization cases",
    )
    parser.add_argument(
        "--unknown-keyword-set-probe",
        action="store_true",
        help="include one unresolved property assignment",
    )
    parser.add_argument(
        "--unknown-keyword-overflow-probe",
        action="store_true",
        help="include 1,025 unique unresolved properties to exercise the cap",
    )
    parser.add_argument(
        "--unknown-keyword-admin-probe",
        action="store_true",
        help="invoke SERV.UNKNOWNREPORT from the admin test account",
    )
    parser.add_argument(
        "--unknown-keyword-rejected-probe",
        action="store_true",
        help="include bad-quantity and bad-arguments dispatch results",
    )
    args = parser.parse_args()

    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")

    write_runtime_files(
        root,
        unknown_keyword_report=args.unknown_keyword_report,
        unknown_keyword_report_format=args.unknown_keyword_report_format,
    )
    write_scripts(
        root,
        unknown_newbie=args.unknown_newbie,
        unknown_keyword_probe=args.unknown_keyword_probe,
        unknown_keyword_set_probe=args.unknown_keyword_set_probe,
        unknown_keyword_normalization_probe=args.unknown_keyword_normalization_probe,
        unknown_keyword_overflow_probe=args.unknown_keyword_overflow_probe,
        unknown_keyword_admin_probe=args.unknown_keyword_admin_probe,
        unknown_keyword_rejected_probe=args.unknown_keyword_rejected_probe,
    )
    write_mul_fixture(root)
    print(f"wrote synthetic Sphere runtime fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
