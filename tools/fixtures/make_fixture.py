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
SYNTHETIC_HAIR_ID = 0x203B


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


def write_mul_fixture(root: Path, *, extra_item_id: int = 0) -> None:
    muls = root / "muls"
    map_blocks = MAP_BLOCKS_X * MAP_BLOCKS_Y
    write_sparse(muls / "map0.mul", map_blocks * MAP_BLOCK_BYTES)
    write_sparse(muls / "staidx0.mul", map_blocks * 12)
    write_bytes(muls / "statics0.mul", b"")
    write_bytes(muls / "multi.idx", b"\0" * 12)
    write_bytes(muls / "multi.mul", b"")

    # Include the protocol fixture's container and character-create hair IDs;
    # all terrain and other item records remain zero/default data.
    tiledata = muls / "tiledata.mul"
    write_sparse(
        tiledata,
        tiledata_size(max(DEFAULT_ITEM_ID, SYNTHETIC_HAIR_ID, extra_item_id)),
    )
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
    world_load_counts_probe: bool = False,
    typedef_container_probe: bool = False,
    multi_property_probe: bool = False,
    named_resource_id_probe: bool = False,
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
                "ARG(unknown_report_comma,one,two)",
                "SYSMESSAGE SPHERE_UNKNOWN_ARG_COMMA <ARG.unknown_report_comma>",
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
    world_load_counts_probe_script = (
        "SYSMESSAGE SPHERE_WORLD_COUNTS <SERV.WORLDCOUNTS>\n"
        if world_load_counts_probe
        else ""
    )
    typedef_container_table = (
        "\n[TYPEDEFS]\nT_NORMAL 0\nT_CONTAINER 1\n"
        if typedef_container_probe
        else ""
    )
    typedef_normal_alias = (
        ""
        if typedef_container_probe
        else "DEFNAME=T_NORMAL\n"
    )
    typedef_container_itemdef = (
        "\n[ITEMDEF 0x0E74]\n"
        "DEFNAME=SYNTHETIC_TYPEDEF_CONTAINER\n"
        "NAME=synthetic typedef container\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        if typedef_container_probe
        else ""
    )
    multi_property_typedef = (
        "\n[TYPEDEF 47]\nDEFNAME=T_MULTI\n"
        if multi_property_probe
        else ""
    )
    multi_property_itemdef = (
        "\n[ITEMDEF 0x4000]\n"
        "DEFNAME=SYNTHETIC_MULTI\n"
        "NAME=synthetic multi\n"
        "TYPE=T_MULTI\n"
        "MULTIREGION=-1,-1,1,1\n"
        if multi_property_probe
        else ""
    )
    named_resource_id_probe_sections = ""
    if named_resource_id_probe:
        sections = [
            "[ITEMDEF 0x0A000]\n"
            "DEFNAME=SYNTHETIC_ALLOC_RESERVED_ITEM\n"
            "NAME=reserved synthetic item ID\n"
            "TYPE=T_NORMAL\n",
        ]
        for index in range(12):
            name = f"SYNTHETIC_ALLOC_ITEM_{index:02d}"
            sections.append(
                f"[ITEMDEF {name}]\n"
                f"DEFNAME={name}\n"
                f"NAME=synthetic allocated item {index}\n"
                "TYPE=T_NORMAL\n"
            )
        sections.append(
            "[CHARDEF 0x06000]\n"
            "DEFNAME=SYNTHETIC_ALLOC_RESERVED_CHAR\n"
            "NAME=reserved synthetic character ID\n"
            "ID=0x0190\n"
            "STR=100\n"
            "DEX=100\n"
        )
        for index in range(12):
            name = f"SYNTHETIC_ALLOC_CHAR_{index:02d}"
            sections.append(
                f"[CHARDEF {name}]\n"
                f"DEFNAME={name}\n"
                f"NAME=synthetic allocated character {index}\n"
                "ID=0x0190\n"
                "STR=100\n"
                "DEX=100\n"
            )
        sections.extend(
            [
                "[ITEMDEF SYNTHETIC_ALLOC_CONTAINER]\n"
                "DEFNAME=SYNTHETIC_ALLOC_CONTAINER\n"
                "NAME=synthetic allocated container\n"
                "TYPE=CONTAINER\n"
                "TDATA2=1\n",
                "[ITEMDEF SYNTHETIC_ALLOC_CONTENT]\n"
                "DEFNAME=SYNTHETIC_ALLOC_CONTENT\n"
                "NAME=synthetic allocated container content\n"
                "TYPE=T_NORMAL\n",
            ]
        )
        named_resource_id_probe_sections = "\n" + "\n".join(sections)
    write_text(
        root / "scripts" / "spheretables.scp",
        """; Synthetic definitions generated by tools/fixtures/make_fixture.py.
; This file contains no client or shard data.

""" + typedef_container_table + """
[TYPEDEF 0]
""" + typedef_normal_alias + """
[TYPEDEF 1]
DEFNAME=CONTAINER

[TYPEDEF 61]
DEFNAME=T_HAIR

[ITEMDEF 0x0E75]
DEFNAME=DEFAULTITEM
NAME=synthetic container
TYPE=CONTAINER
TDATA2=1

[ITEMDEF 0x0E76]
DEFNAME=SYNTHETIC_OBJECT
NAME=synthetic object
TYPE=T_NORMAL

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
VAR dotted_getter_calls,0
f_fixture_getter.sysmessage SPHERE_DOTTED_METHOD_REACHED
SYSMESSAGE SPHERE_DOTTED_METHOD_COUNT <VAR(dotted_getter_calls)>
VAR dotted_getter_calls,0
SYSMESSAGE SPHERE_DOTTED_PROPERTY <f_fixture_getter.name>
SYSMESSAGE SPHERE_DOTTED_PROPERTY_COUNT <VAR(dotted_getter_calls)>
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
""" + world_load_counts_probe_script + unknown_keyword_probe_script + unknown_keyword_overflow_script + typedef_container_itemdef + multi_property_typedef + multi_property_itemdef + """
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
ARG(comma_text,first,second,third)
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
SYSMESSAGE SPHERE_ARG_COMMA_COMMAND <ARG.comma_text>
SYSMESSAGE SPHERE_ARG_COMMA_EXPRESSION <ARG(expr_comma_text,alpha,beta,gamma)>|<ARG.expr_comma_text>
SYSMESSAGE SPHERE_ARG_LENGTH <?STRLEN(<ARG.text>)?>
SYSMESSAGE SPHERE_ARG_DEFAULT [<ARG.unset_value>]
RETURN 10

[FUNCTION f_fixture_getter]
VAR dotted_getter_calls,<EVAL <VAR(dotted_getter_calls)>+1>
RETURN <SRC.SERIAL>

[SPEECH spk_AllPlayers]

[AREA Synthetic world]
P=128,128,0
RECT=1,1,6143,4096

""" + skill_sections() + """

[NEWBIE MAGERY]
ITEMNEWBIE=0x0E72

[NEWBIE resist]
ITEMNEWBIE=0x0E73
""" + unknown_newbie_section + named_resource_id_probe_sections,
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


def write_world_load_counts_save(
    root: Path,
    *,
    truncate_world_item: bool,
    unresolved_worldchar_type: bool,
    noncontainer_reference: bool,
    typedef_container_reference: bool,
    multi_property_reference: bool,
    named_container_reference: bool,
) -> None:
    """Write a synthetic save with one selected world-load scenario."""

    world_sections = [
        "TITLE=Sphere synthetic object-count fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    if named_container_reference:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_ALLOC_CONTAINER]",
                "SERIAL=4",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_ALLOC_CONTENT]",
                "SERIAL=5",
                "CONT=4",
            ]
        )
    elif noncontainer_reference:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=4",
                "P=128,128,0",
                "[WORLDITEM DEFAULTITEM]",
                "SERIAL=5",
                "CONT=4",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=6",
                "CONT=5",
            ]
        )
    elif typedef_container_reference:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_TYPEDEF_CONTAINER]",
                "SERIAL=4",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=5",
                "CONT=4",
            ]
        )
    elif multi_property_reference:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=4",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_MULTI]",
                "SERIAL=5",
                "P=128,128,0",
            ]
        )
    else:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_MAGERY_START]",
                "SERIAL=1",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_RESIST_START]",
                "SERIAL=2",
                "P=129,128,0",
            ]
        )
    if truncate_world_item:
        world_sections.extend(
            [
                "[WORLDITEM]",
                "NAME=deliberately-truncated-synthetic-object",
            ]
        )
    world_sections.append("[EOF]")
    write_text(root / "save" / "sphereworld.scp", "\n".join(world_sections))
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic object-count fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR SYNTHETIC_MISSING_CHARDEF]"
                if unresolved_worldchar_type
                else (
                    "[WORLDCHAR SYNTHETIC_ALLOC_CHAR_00]"
                    if named_container_reference
                    else "[WORLDCHAR c_MAN]"
                ),
                "SERIAL=3",
                "NPC=2",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=130,128,0",
                "[EOF]",
            ]
        ),
    )


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
        help="include a valid comma-valued ARG and an invalid empty-name ARG",
    )
    parser.add_argument(
        "--world-load-counts",
        action="store_true",
        help="write a synthetic save with two items and one character",
    )
    parser.add_argument(
        "--world-load-counts-probe",
        action="store_true",
        help="invoke SERV.WORLDCOUNTS from the admin login event",
    )
    parser.add_argument(
        "--truncate-world-item",
        action="store_true",
        help="append one incomplete world item section to the synthetic save",
    )
    parser.add_argument(
        "--unresolved-worldchar-type",
        action="store_true",
        help="use one synthetic character type with no CHARDEF",
    )
    parser.add_argument(
        "--noncontainer-reference",
        action="store_true",
        help="include a nested item that references a non-container",
    )
    parser.add_argument(
        "--typedef-container-reference",
        action="store_true",
        help="include a symbolic item type from the built-in TYPEDEFS table",
    )
    parser.add_argument(
        "--multi-property",
        action="store_true",
        help="load one synthetic IT_MULTI item through its P property",
    )
    parser.add_argument(
        "--named-resource-ids",
        action="store_true",
        help="add named ITEMDEF/CHARDEF entries and a nested named-container save",
    )
    args = parser.parse_args()

    world_load_modes = (
        args.truncate_world_item,
        args.unresolved_worldchar_type,
        args.noncontainer_reference,
        args.typedef_container_reference,
        args.multi_property,
        args.named_resource_ids,
    )
    if any(world_load_modes) and not args.world_load_counts:
        parser.error("world-load options require --world-load-counts")
    if sum(world_load_modes) > 1:
        parser.error("choose only one world-load fixture mode")

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
        world_load_counts_probe=args.world_load_counts_probe,
        typedef_container_probe=args.typedef_container_reference,
        multi_property_probe=args.multi_property,
        named_resource_id_probe=args.named_resource_ids,
    )
    if args.world_load_counts:
        write_world_load_counts_save(
            root,
            truncate_world_item=args.truncate_world_item,
            unresolved_worldchar_type=args.unresolved_worldchar_type,
            noncontainer_reference=args.noncontainer_reference,
            typedef_container_reference=args.typedef_container_reference,
            multi_property_reference=args.multi_property,
            named_container_reference=args.named_resource_ids,
        )
    write_mul_fixture(root)
    print(f"wrote synthetic Sphere runtime fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
