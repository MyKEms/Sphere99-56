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
TIMER_LIFETIME_OWNER_SERIAL = 100
TIMER_LIFETIME_ITEM_SERIALS = (101, 102, 103, 104, 105)
UID_F_ITEM = 0x40000000
TIMER_LIFETIME_DELAY_SECONDS = 15
TIMER_LIFETIME_OBSERVER_DELAY_SECONDS = 22

# Dotted-expression probe.  Each row is (key, expression, contexts): "C" runs
# the expression in the player's login trigger (default object and SRC are the
# character), "I" in the @Equip trigger of an equipped item (default object is
# the item, SRC is the character).  tools/fixtures/test_dotted_expressions.py
# holds the expected values.
DOTTED_PROBE_ACCOUNT = "DottedProbe"
DOTTED_PROBE_ITEM_ID = 0x0E7B
DOTTED_PROBE_DISPOSABLE_ID = 0x0E7C
DOTTED_PROBE_LAYER = 30
DOTTED_PROBE_SECTOR_LIGHT = 4
DOTTED_PROBE_MARKER = "SPHERE_DOTTED_EXPR"
DOTTED_EXPRESSION_ROWS = (
    # Forms without a function-root chain; their results must not change.
    ("src_name", "<src.name>", "CI"),
    ("src_str", "<src.str>", "C"),
    ("src_serial", "<src.serial>", "CI"),
    ("serv_name", "<serv.name>", "C"),
    ("var_paren", "<var(dotted_probe_var)>", "C"),
    ("eval_decimal", "<eval 5*1.5>", "CI"),
    ("eval_decimal_zero", "<eval 30.0>", "C"),
    ("eval_paren_decimal", "<eval(2.5)>", "C"),
    ("eval_nested", "<eval <src.str>*1.5>", "C"),
    ("strlen_dot", "<strlen a.b>", "CI"),
    ("strcmp_dot", "<strcmp a.b,a.b>", "C"),
    ("strindexof_dot", "<strindexof abc.def,def>", "C"),
    ("safe_src_name", "<safe src.name>", "C"),
    ("safe_missing_tag", "<safe src.tag(probe_missing)>", "C"),
    ("tag_paren", "<tag(probe_text)>", "CI"),
    ("function_plain", "<f_dotted_serial>", "CI"),
    ("serial", "<serial>", "I"),
    ("deferred_src_tag", "<?src.tag(probe_text)?>", "C"),
    ("deferred_eval", "<?eval 5*1.5?>", "C"),
    ("deferred_strlen", "<?strlen a.b?>", "C"),
    # One-level references whose last segment carries its own arguments or
    # is a script function evaluated with the reference as default object.
    ("src_tag_paren", "<src.tag(probe_text)>", "CI"),
    ("src_tag_paren_num", "<src.tag(probe_num)>", "C"),
    ("src_tag_paren_missing", "<src.tag(probe_missing)>", "C"),
    ("src_function", "<src.f_dotted_serial>", "CI"),
    ("src_function_args", "<src.f_dotted_arg(5)>", "C"),
    ("function_root_tag", "<f_dotted_serial.tag(probe_text)>", "CI"),
    ("finduid_missing_name", "<finduid(0bad0bad).name>", "C"),
    # Function roots with arguments and multi-level chains.
    ("src_account_name", "<src.account.name>", "CI"),
    ("findaccount_name", "<findaccount(" + DOTTED_PROBE_ACCOUNT + ").name>", "C"),
    ("finduid_name", "<finduid(<src.serial>).name>", "C"),
    ("finduid_serial", "<finduid(<src.serial>).serial>", "C"),
    ("finduid_tag", "<finduid(<src.serial>).tag(probe_text)>", "C"),
    ("finduid_function", "<finduid(<src.serial>).f_dotted_serial>", "C"),
    ("lastnewitem_name", "<serv.lastnewitem.name>", "C"),
    ("function_args_root", "<f_dotted_arg(<src.serial>).name>", "C"),
    ("function_args_chain", "<f_dotted_arg(<src.serial>).findlayer(30).serial>", "C"),
    ("src_findlayer_name", "<src.findlayer(30).name>", "CI"),
    ("src_findlayer_serial", "<src.findlayer(30).serial>", "CI"),
    ("src_findlayer_tag", "<src.findlayer(30).tag(probe_text)>", "C"),
    ("src_sector_light", "<src.sector.light>", "C"),
    ("deferred_finduid_name", "<?finduid(<src.serial>).name?>", "C"),
    ("deferred_findlayer_serial", "<?src.findlayer(30).serial?>", "C"),
    # Chains rooted at a reference property of the default object, and
    # dotted TAG.name / TAG0.name reads.
    ("sector_light", "<sector.light>", "C"),
    ("cont_name", "<cont.name>", "I"),
    ("cont_tag", "<cont.tag(probe_text)>", "I"),
    ("topobj_name", "<topobj.name>", "I"),
    ("tag_dot", "<tag.probe_text>", "CI"),
    ("src_tag_dot", "<src.tag.probe_text>", "CI"),
    ("src_tag_dot_missing", "<src.tag.probe_missing>", "C"),
    ("src_tag0_dot_missing", "<src.tag0.probe_missing>", "C"),
    ("src_tag0_dot_num", "<src.tag0.probe_num>", "C"),
    # Bare reference operands in numeric expressions.
    ("eval_bare", "<eval src.str+1>", "C"),
    ("eval_bare_mixed", "<eval <src.str>+src.dex>", "C"),
    ("eval_bare_negative", "<eval -src.str>", "C"),
    ("eval_bare_tag", "<eval src.tag.probe_num*2>", "CI"),
    ("eval_bare_function", "<eval src.f_dotted_serial>", "I"),
    ("eval_bracket_str_dex", "<eval <src.str>+<src.dex>>", "C"),
    ("eval_defname", "<eval dotted_probe_const>", "C"),
    ("eval_unknown_reference", "<eval foo.bar>", "C"),
    # Expression grammar: parentheses, unary !, && and ||.  Arithmetic and
    # comparison operators still chain from left to right without
    # precedence.
    ("eval_paren_group", "<eval (1+2)*3>", "C"),
    ("eval_paren_right", "<eval 2*(3+4)>", "C"),
    ("eval_not", "<eval !0>", "C"),
    ("eval_and", "<eval 1 && 1>", "C"),
    ("eval_or_false", "<eval 0 || 0>", "C"),
    ("eval_chain_left", "<eval 10-3-2>", "C"),
    ("eval_chain_no_precedence", "<eval 1+2*3>", "C"),
    ("eval_chain_compare", "<eval 3==1+2>", "C"),
    # Unresolved on every build so far (VAR.name reads are not implemented).
    # Not asserted; it checks that the unknown-keyword report keeps the
    # normalized legacy key.
    ("unresolved_var_dot", "<var.dotted_probe_var>", "C"),
)

# Numeric conditions with bare reference operands: (key, condition).  The
# probe prints 1 when IF takes the condition as true and 0 otherwise.
DOTTED_CONDITION_ROWS = (
    ("cond_src_str_eq", "(src.str==<src.str>)"),
    ("cond_src_str_gt", "(src.str>10)"),
    ("cond_src_str_lt", "(src.str<10)"),
    ("cond_chain_arith", "(src.str+5>src.dex)"),
    ("cond_sector_light", "(sector.light==4)"),
    ("cond_tag_set", "(src.tag.probe_num==7)"),
    ("cond_tag_paren", "(src.tag(probe_num)==7)"),
    ("cond_tag_unset", "(src.tag.probe_missing==1)"),
    ("cond_tag0_unset", "(src.tag0.probe_missing==0)"),
    ("cond_base_tag", "(tag.probe_num==7)"),
    ("cond_findlayer", "(src.findlayer(30))"),
    ("cond_findlayer_empty", "(src.findlayer(9))"),
    ("cond_finduid_name", "(finduid(<src.serial>).name==<src.name>)"),
    ("cond_bare_name_other", "(src.name==Other)"),
    ("cond_bracket_name_other", "(<src.name>==Other)"),
    ("cond_defname", "(dotted_probe_const==1234)"),
    ("cond_unknown_reference", "(foo.bar)"),
    ("cond_bare_and", "(src.str>10) && (src.dex>10)"),
    ("cond_bare_paren", "((src.str+5)>src.dex)"),
    ("cond_bare_not", "(!src.tag.probe_missing)"),
    # Expression grammar in conditions.
    ("grammar_and_true", "(1>0) && (2>1)"),
    ("grammar_and_false", "(1>0) && (2<1)"),
    ("grammar_or_true", "(0) || (1)"),
    ("grammar_or_false", "(0) || (0)"),
    ("grammar_not_zero", "(!0)"),
    ("grammar_not_one", "(!1)"),
    ("grammar_not_paren", "(!(1>2))"),
    ("grammar_paren_arith", "(((2+3)*4)==20)"),
    ("grammar_and_or", "(0) && (1) || (1)"),
    ("grammar_or_and", "(1) || (0) && (0)"),
    ("grammar_ge", "(5 >= 3)"),
    ("grammar_ge_equal", "(3 >= 3)"),
    ("grammar_ge_false", "(2 >= 3)"),
    ("grammar_le_equal", "(3 <= 3)"),
    ("grammar_le_false", "(4 <= 3)"),
    ("grammar_unparenthesized", "(1 < 2 && 3 > 2)"),
    ("grammar_escape_terms", "(<src.str> > 10) && (<src.tag(probe_num)> == 7)"),
    ("grammar_bare_terms", "(src.str>10) && (src.tag.probe_num==7)"),
    ("grammar_bare_terms_false", "(src.str>10) && (src.tag.probe_num==8)"),
    ("grammar_not_bare_set", "(!src.tag.probe_num)"),
)


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


def dotted_expression_lines(context: str, emit: str) -> list[str]:
    return [
        f"{emit} {DOTTED_PROBE_MARKER} {context}|{key}|[{expression}]"
        for key, expression, contexts in DOTTED_EXPRESSION_ROWS
        if context in contexts
    ]


def dotted_expression_scripts() -> tuple[str, str, str]:
    """Return the login lines, extra sections and @EnvironChange body."""

    marker = DOTTED_PROBE_MARKER
    login = [
        "TAG.probe_text=chartext",
        "TAG.probe_num=7",
        "VAR dotted_probe_var,globalvalue",
        "NEWITEM SYNTHETIC_DOTTED_DISPOSABLE",
        "EQUIPLAST",
        "NEWITEM SYNTHETIC_DOTTED_PROBE",
        "EQUIPLAST",
    ]
    login += dotted_expression_lines("C", "SYSMESSAGE")
    login += [
        # Commands whose left side is a reference.
        "TAG.cmd_base_set=23",
        "SRC.TAG.cmd_src_set=21",
        "F_DOTTED_SERIAL.TAG.cmd_function_set=30",
        "FINDUID(1).TAG.cmd_finduid_set=41",
        "SRC.SYSMESSAGE " + marker + " C|cmd_src_method|[reached]",
        "SYSMESSAGE " + marker + " C|cmd_readback|[<tag(cmd_base_set)>|<tag(cmd_src_set)>|"
        "<tag(cmd_function_set)>|<tag(cmd_finduid_set)>|<tag(cmd_item_src_set)>]",
        "SRC.NAME=DottedRenamed",
        "SYSMESSAGE " + marker + " C|cmd_src_name_set|[<name>]",
        "NAME=" + DOTTED_PROBE_ACCOUNT,
        "SYSMESSAGE " + marker + " C|disposable_before|[<isuidvalid <f_dotted_disposable>>]",
        "F_DOTTED_DISPOSABLE.REMOVE",
        "SYSMESSAGE " + marker + " C|disposable_after|[<isuidvalid <f_dotted_disposable>>]",
        # A reference-returning function root is evaluated exactly once per
        # expression, including when its suffix does not resolve or is a
        # method with side effects (DUPE creates one character per call).
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|getter_unknown|[<f_fixture_getter.UNKNOWN_REVIEW_PROPERTY>]",
        "SYSMESSAGE " + marker + " C|getter_unknown_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|getter_malformed|[<f_fixture_getter.UNKNOWN_REVIEW_PROPERTY.>]",
        "SYSMESSAGE " + marker + " C|getter_malformed_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|getter_reference|[<f_fixture_getter.name>]",
        "SYSMESSAGE " + marker + " C|getter_reference_count|[<VAR(dotted_getter_calls)>]",
        "SYSMESSAGE " + marker + " C|dupe_chars_before|[<SERV.CHARS>]",
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|dupe_reference|[<f_fixture_getter.DUPE>]",
        "SYSMESSAGE " + marker + " C|dupe_reference_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|dupe_value|[<f_fixture_getter.DUPE.SERIAL>]",
        "SYSMESSAGE " + marker + " C|dupe_value_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "SYSMESSAGE " + marker + " C|dupe_value_valid|[<ISUIDVALID 0x<f_fixture_getter.DUPE.SERIAL>>]",
        "SYSMESSAGE " + marker + " C|dupe_value_valid_count|[<VAR(dotted_getter_calls)>]",
        "SYSMESSAGE " + marker + " C|dupe_chars_after|[<SERV.CHARS>]",
    ]
    for key, condition in DOTTED_CONDITION_ROWS:
        login += [
            f"IF {condition}",
            f"SYSMESSAGE {marker} C|{key}|[1]",
            "ELSE",
            f"SYSMESSAGE {marker} C|{key}|[0]",
            "ENDIF",
        ]
    login += [
        # A bare reference operand runs a script-function root once per
        # evaluation, and a WHILE condition re-reads the reference each time.
        "VAR dotted_getter_calls,0",
        "IF (f_fixture_getter.name==<src.name>)",
        "ENDIF",
        "SYSMESSAGE " + marker + " C|cond_function_root_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "IF (f_fixture_getter(1))",
        "ENDIF",
        "SYSMESSAGE " + marker + " C|cond_function_call_count|[<VAR(dotted_getter_calls)>]",
        # && and || evaluate both operands, as <...> operands on one line
        # are all expanded before the condition is read.
        "VAR dotted_getter_calls,0",
        "IF (0) && (f_fixture_getter.name)",
        "ENDIF",
        "SYSMESSAGE " + marker + " C|grammar_and_operand_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "IF (1) || (f_fixture_getter.name)",
        "ENDIF",
        "SYSMESSAGE " + marker + " C|grammar_or_operand_count|[<VAR(dotted_getter_calls)>]",
        "VAR dotted_getter_calls,0",
        "IF (0) && (<f_fixture_getter.name>)",
        "ENDIF",
        "SYSMESSAGE " + marker + " C|grammar_escape_operand_count|[<VAR(dotted_getter_calls)>]",
        "IF (0)",
        "SYSMESSAGE " + marker + " C|grammar_elif|[if]",
        "ELIF (1) && (2>1)",
        "SYSMESSAGE " + marker + " C|grammar_elif|[elif]",
        "ELSE",
        "SYSMESSAGE " + marker + " C|grammar_elif|[else]",
        "ENDIF",
        "WHILE (src.tag0.probe_grammar_loop<3) && (1)",
        "SRC.TAG.probe_grammar_loop=<EVAL <src.tag0.probe_grammar_loop>+1>",
        "ENDWHILE",
        "SYSMESSAGE " + marker + " C|grammar_while|[<src.tag0.probe_grammar_loop>]",
        "WHILE (src.tag0.probe_loop<3)",
        "SRC.TAG.probe_loop=<EVAL <src.tag0.probe_loop>+1>",
        "ENDWHILE",
        "SYSMESSAGE " + marker + " C|while_bare_reference|[<src.tag0.probe_loop>]",
        "SYSMESSAGE " + marker + " C|environ_calls|[<VAR(environ_calls)>]",
        "SYSMESSAGE " + marker + " C|environ_max_depth|[<VAR(environ_max_depth)>]",
        "SYSMESSAGE " + marker + "_END",
    ]

    # An @EnvironChange handler that keeps its sector at a fixed light level
    # behind a bare SECTOR.LIGHT guard.  Setting a sector light re-runs
    # @EnvironChange for the characters there, so the handler must stop
    # re-entering itself once the level is in effect, whether or not its
    # guard reads the level.
    light = DOTTED_PROBE_SECTOR_LIGHT
    environ_change = "\n".join(
        [
            "VAR environ_calls,<EVAL <VAR(environ_calls)>+1>",
            "VAR environ_depth,<EVAL <VAR(environ_depth)>+1>",
            "IF (<VAR(environ_depth)> > <EVAL <VAR(environ_max_depth)>>)",
            "VAR environ_max_depth,<VAR(environ_depth)>",
            "ENDIF",
            f"IF (sector.light=={light})",
            "ELSE",
            f"SECTOR.LIGHT={light}",
            "ENDIF",
            "VAR environ_depth,<EVAL <VAR(environ_depth)>-1>",
            "RETURN",
        ]
    ) + "\n"

    equip = ["TAG.probe_text=itemtext"]
    equip += dotted_expression_lines("I", "SRC.SYSMESSAGE")
    equip += [
        "SRC.TAG.cmd_item_src_set=11",
        "F_DOTTED_SERIAL.TAG.cmd_item_function_set=18",
        "SRC.SYSMESSAGE " + marker + " I|cmd_readback|[<tag(cmd_item_function_set)>]",
    ]

    sections = (
        f"\n[ITEMDEF 0x{DOTTED_PROBE_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_DOTTED_PROBE\n"
        "NAME=synthetic dotted probe\n"
        "TYPE=T_EQ_SCRIPT\n"
        f"LAYER={DOTTED_PROBE_LAYER}\n"
        "ON=@Equip\n" + "\n".join(equip) + "\n"
        f"\n[ITEMDEF 0x{DOTTED_PROBE_DISPOSABLE_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_DOTTED_DISPOSABLE\n"
        "NAME=synthetic dotted disposable\n"
        "TYPE=T_EQ_SCRIPT\n"
        f"LAYER={DOTTED_PROBE_LAYER}\n"
        "ON=@Create\n"
        "VAR dotted_disposable,<SERIAL>\n"
        "\n[DEFNAMES dotted_probe]\n"
        "dotted_probe_const 1234\n"
        "\n[FUNCTION f_dotted_serial]\n"
        "RETURN <SERIAL>\n"
        "\n[FUNCTION f_dotted_arg]\n"
        "RETURN <ARGS>\n"
        "\n[FUNCTION f_dotted_disposable]\n"
        "RETURN <VAR(dotted_disposable)>\n"
    )
    return "\n".join(login) + "\n", sections, environ_change


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
    world_save_probe: bool = False,
    unresolved_worldchar_type: bool = False,
    typedef_container_probe: bool = False,
    multi_property_probe: bool = False,
    named_resource_id_probe: bool = False,
    timer_lifetime_probe: bool = False,
    dotted_expression_probe: bool = False,
    timer_lifetime_item_first_probe: bool = False,
) -> None:
    timer_lifetime_probe = timer_lifetime_probe or timer_lifetime_item_first_probe
    unknown_newbie_section = (
        "\n[NEWBIE SYNTHETIC_UNKNOWN_SKILL]\nITEMNEWBIE=0x0E72\n"
        if unknown_newbie
        else ""
    )
    dotted_expression_login, dotted_expression_sections, environ_change_body = (
        dotted_expression_scripts() if dotted_expression_probe else ("", "", "RETURN\n")
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
    world_save_probe_script = (
        "SERV.SAVE\n"
        if world_save_probe
        else ""
    )
    timer_lifetime_before_markers = (
        "SERV.B SPHERE_TIMER_COUNTS_BEFORE <SERV.ITEMS>|<SERV.CHARS>\n"
        "SERV.B SPHERE_TIMER_UIDS_BEFORE "
        "<ISUIDVALID 100>|<ISUIDVALID 1073741925>|"
        "<ISUIDVALID 1073741926>|<ISUIDVALID 1073741927>|"
        "<ISUIDVALID 1073741928>|<ISUIDVALID 1073741929>\n"
        if timer_lifetime_probe
        else ""
    )
    timer_lifetime_probe_itemdefs = (
        "\n[ITEMDEF 0x0E78]\n"
        "DEFNAME=SYNTHETIC_TIMER_OBSERVER\n"
        "NAME=synthetic timer observer\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        f"ON=@Create\nSERV.B SPHERE_TIMER_OBSERVER_CREATED\nTIMER={TIMER_LIFETIME_OBSERVER_DELAY_SECONDS}\n"
        f"ON=@Equip\nSERV.B SPHERE_TIMER_OBSERVER_EQUIPPED <TIMER>|<LAYER>\nTIMER={TIMER_LIFETIME_OBSERVER_DELAY_SECONDS}\nSERV.B SPHERE_TIMER_OBSERVER_ARMED <TIMER>|<LAYER>\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_TIMER_LISTENER_ALIVE\n"
        "SERV.SAVE 1\n"
        "SERV.B SPHERE_TIMER_COUNTS_AFTER <SERV.ITEMS>|<SERV.CHARS>\n"
        "SERV.B SPHERE_TIMER_UIDS_AFTER "
        "<ISUIDVALID 100>|<ISUIDVALID 1073741925>|"
        "<ISUIDVALID 1073741926>|<ISUIDVALID 1073741927>|"
        "<ISUIDVALID 1073741928>|<ISUIDVALID 1073741929>\n"
        "RETURN 0\n"
        "\n[ITEMDEF 0x0E79]\n"
        "DEFNAME=SYNTHETIC_TIMER_SIBLING\n"
        "NAME=synthetic timer sibling\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_TIMER_SIBLING_TRIGGERED\n"
        "RETURN 1\n"
        if timer_lifetime_probe
        else ""
    )
    timer_lifetime_observer_login = (
        "NEWITEM SYNTHETIC_TIMER_OBSERVER\n"
        "EQUIPLAST\n"
        if timer_lifetime_probe
        else ""
    )
    timer_lifetime_baseline = (
        "VAR(timer_probe_before_items,<SERV.ITEMS>)\n"
        "VAR(timer_probe_before_chars,<SERV.CHARS>)\n"
        if timer_lifetime_probe
        else ""
    )
    timer_lifetime_owner_create = (
        "ON=@Create\nITEM=SYNTHETIC_TIMER_LIFETIME\nLAYER=30\nTIMER=5\n"
        if not timer_lifetime_probe
        else ""
    )
    timer_lifetime_item_type = (
        "TYPE=T_CONTAINER\nLAYER=21\nTDATA2=1\n"
        if timer_lifetime_probe
        else "TYPE=T_EQ_SCRIPT\nLAYER=30\n"
    )
    timer_remove = "REMOVE" if timer_lifetime_item_first_probe else "CONT.REMOVE"
    unequip_remove = "CONT.REMOVE" if timer_lifetime_item_first_probe else "REMOVE"
    typedef_container_table = (
        "\n[TYPEDEFS]\nT_NORMAL 0\nT_CONTAINER 1\n"
        if typedef_container_probe or timer_lifetime_probe
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
    default_char_definition = ""
    default_char_defname2 = "DEFNAME2=DEFAULTCHAR\n"
    if unresolved_worldchar_type:
        default_char_definition = "[DEFNAMES HARDCODED]\nDEFAULTCHAR c_MAN\n\n"
        default_char_defname2 = ""
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

[TYPEDEF 176]
DEFNAME=T_EQ_SCRIPT

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

[ITEMDEF 0x0E77]
DEFNAME=SYNTHETIC_TIMER_LIFETIME
NAME=synthetic timer lifetime item
""" + timer_lifetime_item_type + """ON=@Create
SERV.B SPHERE_TIMER_ITEM_CREATED
ON=@Timer
SERV.B SPHERE_TIMER_LIFETIME_TRIGGERED
""" + timer_lifetime_before_markers + timer_remove + """
SERV.B SPHERE_TIMER_REMOVE_RETURNED
RETURN 1
ON=@UnEquip
SERV.B SPHERE_TIMER_UNEQUIP_TRIGGERED
""" + unequip_remove + """
SERV.B SPHERE_TIMER_UNEQUIP_REMOVE_RETURNED

""" + timer_lifetime_probe_itemdefs + """
[ITEMDEF 0x09B2]
DEFNAME=SYNTHETIC_SHIRT
NAME=synthetic shirt
TYPE=T_NORMAL

""" + default_char_definition + """
[CHARDEF 0x0190]
DEFNAME=c_MAN
""" + default_char_defname2 + """NAME=synthetic human
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

[CHARDEF 0x0192]
DEFNAME=SYNTHETIC_TIMER_OWNER
NAME=synthetic timer owner
ID=0x0190
STR=100
DEX=100
""" + timer_lifetime_owner_create + """

[EVENTS e_AllPlayers]
ON=@LogIn
""" + timer_lifetime_baseline + """
""" + timer_lifetime_observer_login + """
ARG(timer_probe_match,<STRMATCH <NAME>,TimerLifetimeProbe>)
IF (<ARG.timer_probe_match> == 1)
NEWNPC SYNTHETIC_TIMER_OWNER
SYSMESSAGE SPHERE_TIMER_OWNER_CREATED
ENDIF
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
""" + ("" if timer_lifetime_probe else "NEWITEM SYNTHETIC_HAIR\n") + """
""" + world_load_counts_probe_script + unknown_keyword_probe_script + unknown_keyword_overflow_script + dotted_expression_login + typedef_container_itemdef + multi_property_typedef + multi_property_itemdef + """
ON=@EnvironChange
""" + environ_change_body + """ON=@Logout
""" + world_save_probe_script + """
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
""" + dotted_expression_sections + """
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
    force_garbage_collect: bool = False,
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
""" + ("FORCEGARBAGECOLLECT=1\n" if force_garbage_collect else "") + unknown_keyword_report_setting + """

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
    rejected_property: bool,
    weird_item: bool,
) -> None:
    """Write a synthetic save with one selected world-load scenario."""

    world_sections = [
        "TITLE=Sphere synthetic object-count fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    if rejected_property:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=4",
                "P=128,128,0",
                "SYNTHETIC_LEGACY_UNUSED=1",
                "[WORLDITEM DEFAULTITEM]",
                "SERIAL=5",
                "P=129,128,0",
                "HITS=10",
            ]
        )
    elif weird_item:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=4",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_OBJECT]",
            ]
        )
    elif named_container_reference:
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
    if rejected_property:
        write_text(
            root / "accounts" / "sphereaccu.scp",
            "\n".join(
                [
                    "[ACCOUNT FixturePlayer]",
                    "PASSWORD=fixture-pw",
                    "LASTCHARUID=4",
                    "CHARUID=4",
                    "[EOF]",
                ]
            ),
        )
        char_sections = [
            "[WORLDCHAR c_MAN]",
            "SERIAL=3",
            "NPC=2",
            "ACTION=MAGERY",
            "STR=100",
            "INT=100",
            "DEX=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            "P=130,128,0",
            "[WORLDCHAR c_MAN]",
            "SERIAL=4",
            "ACCOUNT=FixturePlayer",
            "STR=100",
            "INT=100",
            "DEX=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            "SkillLock.5=1",
            "P=131,128,0",
            "[EOF]",
        ]
    else:
        char_sections = [
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
        if unresolved_worldchar_type:
            char_sections.insert(-1, "OBODY=SYNTHETIC_MISSING_BODY")
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic object-count fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
            ]
            + char_sections
        ),
    )


def write_timer_lifetime_save(root: Path) -> None:
    """Seed a saved NPC with an active timer item and a nested sibling tree."""

    timer_item_serial, container_serial, child_a_serial, child_b_serial, sibling_serial = (
        TIMER_LIFETIME_ITEM_SERIALS
    )
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic timer lifetime fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic timer lifetime fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR SYNTHETIC_TIMER_OWNER]",
                f"SERIAL={TIMER_LIFETIME_OWNER_SERIAL}",
                "NPC=2",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=130,128,0",
                "[WORLDITEM SYNTHETIC_TIMER_SIBLING]",
                f"SERIAL={sibling_serial}",
                f"CONT={TIMER_LIFETIME_OWNER_SERIAL}",
                "LAYER=30",
                f"TIMER={TIMER_LIFETIME_DELAY_SECONDS}",
                "[WORLDITEM SYNTHETIC_TIMER_LIFETIME]",
                f"SERIAL={timer_item_serial}",
                f"CONT={TIMER_LIFETIME_OWNER_SERIAL}",
                "LAYER=21",
                f"TIMER={TIMER_LIFETIME_DELAY_SECONDS}",
                "[WORLDITEM DEFAULTITEM]",
                f"SERIAL={container_serial}",
                f"CONT={UID_F_ITEM | timer_item_serial}",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                f"SERIAL={child_a_serial}",
                f"CONT={UID_F_ITEM | container_serial}",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                f"SERIAL={child_b_serial}",
                f"CONT={UID_F_ITEM | container_serial}",
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
        "--world-save-probe",
        action="store_true",
        help="save the newly logged-in fixture character from its login event",
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
    parser.add_argument(
        "--rejected-property",
        action="store_true",
        help="include one saved meaningful property rejected by the item loader",
    )
    parser.add_argument(
        "--weird-item",
        action="store_true",
        help="include one saved item that is deleted as invalid during load",
    )
    parser.add_argument(
        "--timer-lifetime-probe",
        action="store_true",
        help="seed a timer-owner, nested-item, sibling, and UID-cleanup probe",
    )
    parser.add_argument(
        "--dotted-expression-probe",
        action="store_true",
        help="evaluate dotted reference expressions and commands at login",
    )
    parser.add_argument(
        "--timer-lifetime-item-first-probe",
        action="store_true",
        help="exercise item-first timer removal with reentrant owner removal",
    )
    args = parser.parse_args()

    world_load_modes = (
        args.truncate_world_item,
        args.unresolved_worldchar_type,
        args.noncontainer_reference,
        args.typedef_container_reference,
        args.multi_property,
        args.named_resource_ids,
        args.rejected_property,
        args.weird_item,
    )
    if any(world_load_modes) and not args.world_load_counts:
        parser.error("world-load options require --world-load-counts")
    if sum(world_load_modes) > 1:
        parser.error("choose only one world-load fixture mode")
    if (args.timer_lifetime_probe or args.timer_lifetime_item_first_probe) and any(world_load_modes):
        parser.error("timer-lifetime probe cannot be combined with a world-load mode")
    if args.timer_lifetime_probe and args.timer_lifetime_item_first_probe:
        parser.error("choose only one timer-lifetime probe mode")

    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")

    write_runtime_files(
        root,
        unknown_keyword_report=args.unknown_keyword_report,
        unknown_keyword_report_format=args.unknown_keyword_report_format,
        force_garbage_collect=args.timer_lifetime_probe or args.timer_lifetime_item_first_probe,
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
        world_save_probe=args.world_save_probe,
        unresolved_worldchar_type=args.unresolved_worldchar_type,
        typedef_container_probe=args.typedef_container_reference,
        multi_property_probe=args.multi_property,
        named_resource_id_probe=args.named_resource_ids,
        timer_lifetime_probe=args.timer_lifetime_probe,
        dotted_expression_probe=args.dotted_expression_probe,
        timer_lifetime_item_first_probe=args.timer_lifetime_item_first_probe,
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
            rejected_property=args.rejected_property,
            weird_item=args.weird_item,
        )
    if args.timer_lifetime_probe or args.timer_lifetime_item_first_probe:
        write_timer_lifetime_save(root)
    write_mul_fixture(root)
    print(f"wrote synthetic Sphere runtime fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
