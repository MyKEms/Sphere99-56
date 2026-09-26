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

from fixture_cases import FIXTURE_MODES


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
MEMORY_TIMER_OWNER_SERIAL = 200
MEMORY_TIMER_ITEM_SERIAL = 201
MEMORY_TIMER_DELAY_SECONDS = 15
MEMORY_TIMER_ITEM_UID = UID_F_ITEM | MEMORY_TIMER_ITEM_SERIAL
MEMORY_TIMER_ITEM_ID = 0x0EA3
MEMORY_TIMER_MARKER = "SPHERE_MEMORY_TIMER_TRIGGERED"
MEMORY_TIMER_REMOVED_MARKER = "SPHERE_MEMORY_TIMER_REMOVED"
CHARACTER_CONTENT_ACCOUNT = "CharacterContentProbe"
CHARACTER_CONTENT_PASSWORD = "char_content_pw"
CHARACTER_CONTENT_CHAR_SERIAL = 3
CHARACTER_CONTENT_ITEM_SERIAL = 4
CHARACTER_CONTENT_ITEM_ID = 0x0E9E
CHARACTER_CONTENT_LAYERED_ITEM_SERIAL = 5
CHARACTER_CONTENT_LAYERED_ITEM_ID = 0x0E9F
CHARACTER_CONTENT_MARKER = "SPHERE_CHARACTER_CONTENT"
NAMED_TIMER_ITEM_ID = 0x0E8B
NAMED_TIMER_ITEM_NAME = "synthetic named timer item"
NAMED_MULTI_NAME = "synthetic named multi"
ROUNDTRIP_ITEM_ID = 0x0E9A
ROUNDTRIP_DISP_ID = 0x0E9B
SPAWN_GEM_SERIALS = tuple(range(100, 110))
SPAWN_GEM_ITEM_ID = 0x1EA7
SPAWN_POINT_SERIAL = UID_F_ITEM | 120
SPAWN_POINT_ITEM_ID = 0x0E9C
SPAWN_POINT_PRODUCT_ID = 0x0E9D
SPAWN_POINT_PRODUCT_NAME = "SYNTHETIC_SPAWN_PRODUCT"
SPAWN_POINT_MARKER = "SPHERE_SPAWN_POINT_CREATED"

# Login escape-buffer probe.  The saved character name is deliberately just
# below the engine's item/name limit; expanding it in a near-limit command
# line must be rejected before the in-place suffix shift can overrun the
# CScript line buffer.
ESCAPE_OVERFLOW_ACCOUNT = "EscapeProbe"
ESCAPE_OVERFLOW_PASSWORD = "escape_pw"
ESCAPE_OVERFLOW_MARKER = "ESCAPE_OVERFLOW_AFTER"
ESCAPE_OVERFLOW_NAME = "N" * 256

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
# Loops in the probe that run into the WHILE/FOR iteration limit.
DOTTED_PROBE_CAPPED_WHILE = "WHILE (2>1)"
DOTTED_PROBE_CAPPED_FOR = "FOR 20000"
DOTTED_PROBE_MARKER = "SPHERE_DOTTED_EXPR"

# Named ARG locals and positional-object probe.  The generated login trigger
# creates one synthetic item through a script-level NEWITEMSAFE wrapper, then
# passes its UID into nested functions so ARG/ARGV resolution is exercised in
# the same context shape as a real function call.  The scratch probe mirrors
# the underscore-named locals used by legacy script helpers.
ARG_LOCALS_ACCOUNT = "ArgLocalsProbe"
ARG_LOCALS_MARKER = "SPHERE_ARG_LOCALS"

# Resource-reference array probe.  The login script adds the same event twice,
# reports the resulting list, removes it by name, and saves the character so
# the test covers both in-memory membership and serialized output.
FINDARG_ACCOUNT = "FindArgProbe"
FINDARG_MARKER = "SPHERE_FINDARG"

# Dialog button probe.  The login trigger opens a dialog whose BUTTON section
# holds an ON=@anybutton fallback ahead of numbered ON=<n> entries (cancel
# included), so order in the section must not decide which entry runs.
# tools/fixtures/test_dialog_buttons.py presses each kind of button.
DIALOG_BUTTON_ACCOUNT = "DialogButtonProbe"
DIALOG_BUTTON_MARKER = "SPHERE_DIALOG_BUTTON"
DIALOG_BUTTON_NAME = "d_synthetic_button_probe"
DIALOG_BUTTON_NUMBERED = 5
DIALOG_BUTTON_FALLBACK = 7
DIALOG_BUTTON_SWITCH = 11
DIALOG_BUTTON_TEXT_ID = 3
# A second dialog lays out its controls with argo.<gump>(...) calls, one of
# them written with aligned columns that make it longer than 128 bytes.
DIALOG_ARGO_LAYOUT_NAME = "d_synthetic_argo_layout"
DIALOG_ARGO_BUTTON = 9
DIALOG_ARGO_LONG_FIELDS = (10, 130, 200, 60, 1, 0, 1)
DIALOG_ARGO_CONTROLS = (
    "htmlgump 10 10 200 60 0 1 0",
    f"button 20 80 2151 2152 1 0 {DIALOG_ARGO_BUTTON}",
    "htmlgump " + " ".join(str(field) for field in DIALOG_ARGO_LONG_FIELDS),
)
# A third dialog gates its layout with control flow on TAGs of the source
# character: IF/ELSE picks one of two buttons, a nested IF(...) with ELSEIF
# picks one text, a WHILE over an ARG local emits one text per row and a
# DOSWITCH picks one picture.  Dialogs opened before it: three that decline
# to open (RETURN 1, and a bare RETURN or RETURN 0 before any control), one
# whose layout starts with IF instead of its position, and one that starts
# with an argo.<gump>(...) call and moves itself with argo.setlocation(x,y).
# Only the last three are sent, in DIALOG_FLOW_GUMPS order.
DIALOG_FLOW_LAYOUT_NAME = "d_synthetic_flow_layout"
DIALOG_FLOW_DECLINED_NAMES = (
    "d_synthetic_flow_declined",
    "d_synthetic_flow_guard_bare",
    "d_synthetic_flow_guard_zero",
)
DIALOG_FLOW_IF_FIRST_NAME = "d_synthetic_flow_if_first"
DIALOG_FLOW_ARGO_FIRST_NAME = "d_synthetic_flow_argo_first"
DIALOG_FLOW_RANK = 1
DIALOG_FLOW_ROWS = 3
DIALOG_FLOW_BUTTON = 22
DIALOG_FLOW_DECLINED_CONTROLS = ("resizepic 0 0 5054 100 100",)
DIALOG_FLOW_CONTROLS = (
    "resizepic 0 0 5054 240 300",
    f"button 20 20 2151 2152 1 0 {DIALOG_FLOW_BUTTON}",
    "text 20 40 0 1",
    *(f"text 20 {60 + row * 20} 0 {4 + row}" for row in range(DIALOG_FLOW_ROWS)),
    f"gumppic 200 20 {100 + DIALOG_FLOW_RANK}",
)
# (x, y, controls) of each dialog sent, in order.
DIALOG_FLOW_GUMPS = (
    (0, 0, ("resizepic 0 0 5054 200 100", "button 10 10 2151 2152 1 0 31")),
    (40, 50, ("resizepic 0 0 5054 220 120", "button 10 10 2151 2152 1 0 32")),
    (30, 60, DIALOG_FLOW_CONTROLS),
)

# Region weather probe.  The generated area applies weather overrides to the
# sectors around the saved character, which reads the values back at login.
REGION_WEATHER_ACCOUNT = "RegionWeather"
REGION_WEATHER_MARKER = "SPHERE_REGION_WEATHER"
REGION_WEATHER_RAIN = 37
REGION_WEATHER_COLD = 23

# Sphere accepts 0-prefixed hexadecimal values for DWORD resource properties.
# The probe reads these values back through a CHARDEF reference so it covers
# both property loading and the script-facing property getter.
DWORD_HEX_ACCOUNT = "DwordHexProbe"
DWORD_HEX_MARKER = "SPHERE_DWORD_HEX"
DWORD_HEX_HIGH_NAME = "SYNTHETIC_DWORD_HEX_HIGH"
DWORD_HEX_LOW_NAME = "SYNTHETIC_DWORD_HEX_LOW"
DWORD_HEX_AGE = 0xFABC
# Resource UID of [SKILL 25]: resource flag | (RES_Skill << 25) | index.
# RES_Skill follows the resource tag table order (SphereCommon/cresourcetag.tbl).
RES_SKILL_TYPE = 41
DWORD_HEX_MAGERY_UID = 0x80000000 | (RES_SKILL_TYPE << 25) | 25

# Book probe.  BOOKs with more pages than the 7-bit resource page field holds
# (0.99 reads pages up to 255), a page above that limit that must be rejected
# cleanly, and an ITEMDEF section named by a complete 0.99 resource ID
# (resource flag, the 0.99 ITEMDEF type bits, index 0x0E76).  The long book
# has a named header and every page; the read book has a fixed index so the
# saved book item can refer to it, and only the pages the client reads.
# tools/fixtures/test_book_pages.py holds the checks.
BOOK_PROBE_ACCOUNT = "BookProbe"
BOOK_PROBE_ITEM_ID = 0x0E8E
BOOK_PROBE_ITEM_SERIAL = 400
BOOK_PROBE_NAME = "SYNTHETIC_LONG_BOOK"
BOOK_PROBE_PAGES = 200
BOOK_PROBE_REJECTED_PAGE = 256
BOOK_READ_INDEX = 0x0200
BOOK_READ_NAME = "SYNTHETIC_READ_BOOK"
BOOK_READ_TITLE = "synthetic read book"
BOOK_READ_PAGES = (1, 126, 127, 128, BOOK_PROBE_PAGES)
RES_BOOK_TYPE = 6
FULL_RID_ITEMDEF = "0A2000E76"
FULL_RID_ALIAS = "SYNTHETIC_FULL_RID_ALIAS"
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
    # HVAL keeps 0.99's 32-bit, leading-zero lower-case hexadecimal format
    # in both space-separated and parenthesized function forms.
    ("hval_space", "<hval 0xBEEF>", "CI"),
    ("hval_paren", "<hval(-1)>", "C"),
    # FINDRES returns a typed resource reference so its properties can be
    # read through the same dotted-expression path as world objects.
    ("findres_spell_mana", "<findres(spell,s_fixture_heal).manause>", "C"),
    ("findres_spell_runes", "<findres(spell,s_fixture_heal).runes>", "C"),
    # Nested results are deliberately longer than their source tags.  The
    # trailing text must survive the outer replacement in both trigger
    # contexts.
    ("nested_escape_suffix", "prefix <STRMATCH <NAME>,*> suffix", "CI"),
    ("nested_escape_after_nested", "left <STRLEN <NAME>> right", "CI"),
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
# The sibling-mutation fixture keeps three independent source lists and three
# unrelated destination containers.  The serials are deliberately explicit so
# the generated scripts can assert UID and parent relationships without any
# private world data.
MUTATION_OWNER_SERIALS = (200, 201, 202)
MUTATION_DESTINATION_OWNER_SERIAL = 300
MUTATION_DEST_SERIALS = (210, 211, 212)
MUTATION_KEEP_SERIALS = (220, 221, 222)
MUTATION_A_SERIALS = (230, 240, 250)
MUTATION_B_SERIALS = (231, 241, 251)
MUTATION_C_SERIALS = (232, 242, 253)
MUTATION_C_CHILD_SERIALS = (233, 243, 254)
MUTATION_TIMER_SECONDS = 5
MUTATION_RELATION_TIMER_SECONDS = 18
MUTATION_KEEP_TIMER_SECONDS = 19
MUTATION_OBSERVER_DELAY_SECONDS = 22

# OnTick content traversal probe.  The mutator is inserted before the sibling
# in the owner's live list.  Its timer callback deletes an unrelated character
# first, then the sibling, so the stale sibling next pointer crosses from an
# item into the world delete list on an unsafe walk.
ONTICK_OWNER_SERIAL = 400
ONTICK_VICTIM_SERIAL = 401
ONTICK_MUTATOR_SERIAL = 430
ONTICK_SIBLING_SERIAL = 431
ONTICK_LISTENER_SERIAL = 432
ONTICK_MUTATOR_TIMER_SECONDS = 5
ONTICK_LISTENER_TIMER_SECONDS = 15
SHUTDOWN_PACK_SERIAL = 232
SHUTDOWN_CHILD_SERIAL = 233
SHUTDOWN_INSERT_SERIAL = 234
SHUTDOWN_RUNTIME_PACK_SERIAL = 236
SHUTDOWN_DEST_SERIAL = 210
SHUTDOWN_DEST_OWNER_SERIAL = 300
SHUTDOWN_EVENT_NAME = "t_shutdown_nested"

# Save-generation fixture for current-format object metadata.  The item and
# NPC intentionally use the writer's leading-zero hexadecimal values so the
# test covers the exact 0.99 text representation on the next load.
EVENTS_ATTR_ITEM_SERIAL = 4
EVENTS_ATTR_CHAR_SERIAL = 3
EVENTS_ATTR_CHANGER = 1234
EVENTS_ATTR_MASK = 0x001C
EVENTS_ATTR_VALUES = ("e_AllPlayers", "t_fixture_events", "class_fixture")

# Legacy object metadata fixture.  The value intentionally exceeds the old
# 128-byte tag parser scratch buffer and the two items exercise both spellings
# emitted by 0.99-era saves for named ATTR bits.
LEGACY_METADATA_ITEM_SERIALS = (4, 5)
LEGACY_METADATA_TAG_KEY = "long_roundtrip"
LEGACY_METADATA_TAG_VALUE = "legacy-tag-" + ("0123456789abcdef" * 24)
LEGACY_METADATA_ATTR_KEYS = ("Attr_MoveAlways", "ATTR_MOVEALWAYS")

# Stairs movement probe.  The synthetic tile uses the same climbable/platform
# flags as the 0.99z8 stairs records and exercises the dynamic height path.
MOVEMENT_STAIRS_ACCOUNT = "MovementProbe"
MOVEMENT_STAIRS_PASSWORD = "movement_pw"
MOVEMENT_STAIRS_CHAR_SERIAL = 3
MOVEMENT_STAIRS_SERIAL = 0x40000021
MOVEMENT_STAIRS_ID = 0x0E94
MOVEMENT_STAIRS_POINT = (128, 127, 0)

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
    # Object predicates are valid bare operands in script conditions.  Keep
    # this form explicit so the resolver cannot regress to DEFNAME-only
    # lookup when a predicate has no dotted suffix or call parentheses.
    ("cond_isplayer", "(isplayer)"),
    # Bare literals and resource constants must retain their legacy numeric
    # values when the object-reference resolver is considered first.
    ("cond_literal_one", "(1)"),
    ("cond_literal_zero", "(0)"),
    ("cond_literal_hex", "(0a)"),
    ("cond_defname_bare", "(dotted_probe_const)"),
    ("cond_item_id", "(i_dotted_probe)"),
    ("cond_type_id", "(t_eq_script)"),
    # STR is both a real property and a fixture DEFNAME.  The historical
    # DEFNAME lookup wins, as it did before the bare-reference change.
    ("cond_property_defname", "(str)"),
    # A bare declared function stays on the legacy numeric path: its body is
    # not executed while reading a condition.  An unknown name remains zero.
    ("cond_bare_function", "(f_dotted_bare_probe)"),
    ("cond_unknown_bare", "(dotted_missing_name)"),
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


def write_movement_tile(path: Path, item_id: int, flags: int, height: int) -> None:
    """Write one synthetic movement tile record into tiledata.mul."""

    record_offset = (
        terrain_size()
        + ((item_id // TILE_BLOCK_QTY) * 4)
        + 4
        + (item_id * ITEM_RECORD_BYTES)
    )
    record = struct.pack(
        "<IBBIIHB20s",
        flags,
        1,
        0,
        0,
        0,
        0,
        height,
        b"synthetic movement tile\0".ljust(20, b"\0"),
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


def book_page_lines(page: int) -> list[str]:
    return [f"synthetic page {page}", f"second line of page {page}"]


def book_pages_sections() -> str:
    sections = [
        "[TYPEDEF 19]\nDEFNAME=T_BOOK\n",
        f"[ITEMDEF 0x{BOOK_PROBE_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_BOOK_ITEM\n"
        "NAME=synthetic book\n"
        "TYPE=T_BOOK\n",
        f"[BOOK {BOOK_PROBE_NAME}]\n"
        f"PAGES={BOOK_PROBE_PAGES}\n"
        "TITLE=synthetic long book\n"
        "AUTHOR=fixture\n",
        f"[BOOK 0{BOOK_READ_INDEX:x}]\n"
        f"DEFNAME={BOOK_READ_NAME}\n"
        f"PAGES={BOOK_PROBE_PAGES}\n"
        f"TITLE={BOOK_READ_TITLE}\n"
        "AUTHOR=fixture\n",
    ]
    for page in range(1, BOOK_PROBE_PAGES + 1):
        sections.append(
            f"[BOOK {BOOK_PROBE_NAME} {page}]\n" + "\n".join(book_page_lines(page)) + "\n"
        )
    for page in BOOK_READ_PAGES:
        # The last page is addressed by the numeric index, the others by name.
        book = f"0{BOOK_READ_INDEX:x}" if page == BOOK_READ_PAGES[-1] else BOOK_READ_NAME
        sections.append(
            f"[BOOK {book} {page}]\n" + "\n".join(book_page_lines(page)) + "\n"
        )
    sections.append(
        f"[BOOK {BOOK_PROBE_NAME} {BOOK_PROBE_REJECTED_PAGE}]\n"
        "this page number is out of range\n"
    )
    sections.append(
        f"[ITEMDEF {FULL_RID_ITEMDEF}]\n"
        f"DEFNAME2={FULL_RID_ALIAS}\n"
    )
    return "\n" + "\n".join(sections)


def write_book_pages_save(root: Path) -> None:
    """Place the probe book at the synthetic starting point."""

    header = ["TITLE=Sphere synthetic book fixture", "VERSION=0.99", "SAVECOUNT=0"]
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            header
            + [
                "[WORLDITEM SYNTHETIC_BOOK_ITEM]",
                f"SERIAL={BOOK_PROBE_ITEM_SERIAL}",
                f"MORE1=0{0x80000000 | (RES_BOOK_TYPE << 25) | BOOK_READ_INDEX:x}",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )
    write_text(root / "save" / "spherechars.scp", "\n".join(header + ["[EOF]"]))


def skill_sections(*, dword_hex_probe: bool = False) -> str:
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
            + (
                "ADVRATE=0fffffff,0,0\n"
                if dword_hex_probe and skill_id == 25
                else "ADVRATE=0,0,0\n"
            )
        )
    return "\n".join(sections)


def dotted_expression_lines(context: str, emit: str) -> list[str]:
    return [
        f"{emit} {DOTTED_PROBE_MARKER} {context}|{key}|[{expression}]"
        for key, expression, contexts in DOTTED_EXPRESSION_ROWS
        if context in contexts
    ]


def arg_locals_scripts() -> tuple[str, str]:
    """Return login lines and function sections for ARG-local behavior."""

    marker = ARG_LOCALS_MARKER
    login = [
        "NEWITEMSAFE SYNTHETIC_OBJECT",
        f"SYSMESSAGE {marker} C|lastnew_name|[<LASTNEW.NAME>]",
        "F_ARG_LOCALS_PROBE(<LASTNEW.SERIAL>)",
        f"SYSMESSAGE {marker} C|scratch_return|[<F_ARG_SCRATCH_PROBE(<LASTNEW.SERIAL>)>]",
        f"SYSMESSAGE {marker} C_END",
    ]
    sections = """
[FUNCTION NEWITEMSAFE]
NEWITEM <ARGS>
RETURN <LASTNEW.SERIAL>

[FUNCTION f_arg_locals_probe]
ARG(i,0)
ARG(argobj,<ARGV(0)>)
SYSMESSAGE SPHERE_ARG_LOCALS C|before|[<ARG.i>|<arg(i)>|<i>]
WHILE (<ARG.i> < 3)
ARG(i,#+1)
ENDWHILE
SYSMESSAGE SPHERE_ARG_LOCALS C|counter_after|[<ARG.i>|<arg(i)>|<i>]
WHILE (i < 5)
ARG(i,#+1)
ENDWHILE
SYSMESSAGE SPHERE_ARG_LOCALS C|bare_after|[<i>]
SYSMESSAGE SPHERE_ARG_LOCALS C|object_before|[<argobj.name>|<ARG(argobj).name>|<ARGV(0).TYPE>]
ARGV(0).TYPE=T_NORMAL
SYSMESSAGE SPHERE_ARG_LOCALS C|object_after|[<argobj.type>|<ARGV(0).TYPE>]
RETURN <i>

[FUNCTION f_arg_scratch_probe]
ARG(scratch_1,101)
ARG(scratch_2,202)
ARG(scratch_3,303)
ARG(scratch_4,404)
ARG(scratch_5,505)
ARG(scratch_6,606)
ARG(scratch_obj,<ARGV(0)>)
SYSMESSAGE SPHERE_ARG_LOCALS C|scratch|[<SCRATCH_1>|<scratch_2>|<SCRATCH_3>|<scratch_4>|<SCRATCH_5>|<scratch_6>]
SYSMESSAGE SPHERE_ARG_LOCALS C|scratch_object|[<SCRATCH_OBJ.NAME>|<scratch_obj.type>]
RETURN <scratch_1>-<SCRATCH_2>-<scratch_3>-<SCRATCH_4>-<scratch_5>-<SCRATCH_6>
"""
    return "\n".join(login) + "\n", sections


def findarg_scripts() -> tuple[str, str]:
    """Return a login probe for resource-reference add/remove semantics."""

    login = [
        "EVENTS=e_FindArgProbe",
        "EVENTS=e_FindArgProbe",
        f"SYSMESSAGE {FINDARG_MARKER} after_add|[<EVENTS>]",
        "EVENTS=-e_FindArgProbe",
        f"SYSMESSAGE {FINDARG_MARKER} after_remove|[<EVENTS>]",
        "SERV.SAVE",
        f"SYSMESSAGE {FINDARG_MARKER}_END",
    ]
    sections = """
[EVENTS e_FindArgProbe]
ON=@LogIn
RETURN 0
"""
    return "\n".join(login) + "\n", sections


def dialog_button_scripts(argo_layout: bool = False, flow_layout: bool = False) -> tuple[str, str]:
    """Return the login lines and the sections of the dialog button probe.

    ``argo_layout`` opens the argo.<gump>(...) layout dialog at login instead;
    ``flow_layout`` opens the declining dialog and then the flow-control one.
    """

    marker = DIALOG_BUTTON_MARKER
    name = DIALOG_BUTTON_NAME
    argo_name = DIALOG_ARGO_LAYOUT_NAME
    long_args = ",".join(f"{field:>20}" for field in DIALOG_ARGO_LONG_FIELDS)
    sections = f"""
[DIALOG {name}]
0 0
resizepic 0 0 5054 240 200
button 20 20 2151 2152 1 0 {DIALOG_BUTTON_NUMBERED}
button 20 60 2151 2152 1 0 {DIALOG_BUTTON_FALLBACK}
checkbox 20 100 210 211 0 {DIALOG_BUTTON_SWITCH}
textentry 20 140 180 20 0 {DIALOG_BUTTON_TEXT_ID} 0

[DIALOG {name} TEXT]
initial text

[DIALOG {name} BUTTON]
ON=@anybutton
TAG.dialog_button_seen=any/<ARGN>
SYSMESSAGE {marker} any|<ARGN>|<ARGCHK({DIALOG_BUTTON_SWITCH})>|<ARGTXT({DIALOG_BUTTON_TEXT_ID})>|<ARGO.NAME>
DIALOG {name}
ON={DIALOG_BUTTON_NUMBERED}
SYSMESSAGE {marker} numbered|<ARGN>
DIALOG {name}
ON=0
SYSMESSAGE {marker} cancel|<ARGN>|<TAG.dialog_button_seen>

[DIALOG {argo_name}]
0 0
argo.htmlgump(10,10,200,60,0,1,0)
argo.button(20,80,2151,2152,1,0,{DIALOG_ARGO_BUTTON})
argo.htmlgump({long_args})

[DIALOG {argo_name} TEXT]
argo layout text

[DIALOG {argo_name} BUTTON]
ON={DIALOG_ARGO_BUTTON}
SYSMESSAGE {marker} argo|<ARGN>|<ARGO.NAME>

[DIALOG {DIALOG_FLOW_DECLINED_NAMES[0]}]
0 0
IF (<SRC.TAG.flow_rank> == {DIALOG_FLOW_RANK})
RETURN 1
ENDIF
{DIALOG_FLOW_DECLINED_CONTROLS[0]}

[DIALOG {DIALOG_FLOW_DECLINED_NAMES[1]}]
0 0
IF (<SRC.TAG.flow_rank> == {DIALOG_FLOW_RANK})
RETURN
ENDIF
{DIALOG_FLOW_DECLINED_CONTROLS[0]}

[DIALOG {DIALOG_FLOW_DECLINED_NAMES[2]}]
IF (<SRC.TAG.flow_rank> == {DIALOG_FLOW_RANK})
RETURN 0
ENDIF
{DIALOG_FLOW_DECLINED_CONTROLS[0]}

[DIALOG {DIALOG_FLOW_IF_FIRST_NAME}]
IF (<SRC.TAG.flow_rank> == {DIALOG_FLOW_RANK})
resizepic 0 0 5054 200 100
ELSE
resizepic 0 0 5054 300 100
ENDIF
button 10 10 2151 2152 1 0 31

[DIALOG {DIALOG_FLOW_ARGO_FIRST_NAME}]
argo.resizepic(0,0,5054,220,120)
argo.setlocation(40,50)
button 10 10 2151 2152 1 0 32

[DIALOG {DIALOG_FLOW_LAYOUT_NAME}]
0 0
resizepic 0 0 5054 240 300
IF (<SRC.TAG.flow_rank> >= 2)
argo.button(20,20,2151,2152,1,0,21)
ELSE
argo.button(20,20,2151,2152,1,0,{DIALOG_FLOW_BUTTON})
IF(<SRC.TAG.flow_rank>=={DIALOG_FLOW_RANK})&&(<SRC.TAG.flow_level> > 3)
text 20 40 0 1
ELSEIF (<SRC.TAG.flow_rank> == 0)
text 20 40 0 2
ELSE
text 20 40 0 3
ENDIF
ENDIF
ARG(flow_row,0)
WHILE (<ARG.flow_row> < {DIALOG_FLOW_ROWS})
argo.text(20,<eval 60+(<ARG.flow_row>*20)>,0,<eval 4+<ARG.flow_row>>)
ARG(flow_row,#+1)
ENDWHILE
DOSWITCH <SRC.TAG.flow_rank>
gumppic 200 20 100
gumppic 200 20 101
gumppic 200 20 102
ENDDO
setlocation=30,60

[DIALOG {DIALOG_FLOW_LAYOUT_NAME} TEXT]
flow layout text

[DIALOG {DIALOG_FLOW_LAYOUT_NAME} BUTTON]
ON={DIALOG_FLOW_BUTTON}
SYSMESSAGE {marker} flow|<ARGN>
"""
    if flow_layout:
        login = (
            f"TAG.flow_rank={DIALOG_FLOW_RANK}\n"
            "TAG.flow_level=5\n"
            + "".join(f"DIALOG {name}\n" for name in DIALOG_FLOW_DECLINED_NAMES)
            + f"DIALOG {DIALOG_FLOW_IF_FIRST_NAME}\n"
            + f"DIALOG {DIALOG_FLOW_ARGO_FIRST_NAME}\n"
            + f"DIALOG {DIALOG_FLOW_LAYOUT_NAME}\n"
        )
        return login, sections
    return f"DIALOG {argo_name if argo_layout else name}\n", sections


def dword_hex_scripts() -> tuple[str, str]:
    """Return login lines and definitions for Sphere DWORD hex parsing."""

    marker = DWORD_HEX_MARKER
    login = [
        f"SYSMESSAGE {marker} C|high|[<FINDUID(<{DWORD_HEX_HIGH_NAME}>).ANIM>]",
        f"SYSMESSAGE {marker} C|low|[<FINDUID(<{DWORD_HEX_LOW_NAME}>).ANIM>]",
        "SRC.SPEECHCOLOR=0fabc",
        f"SYSMESSAGE {marker} C|speechcolor|[<SRC.SPEECHCOLOR>]",
        f"SYSMESSAGE {marker} C|advrate|[<FINDUID({DWORD_HEX_MAGERY_UID}).ADVRATE>]",
        f"SYSMESSAGE {marker} C|age|[<SRC.AGE>]",
        "NEWITEM SYNTHETIC_DWORD_HEX_ITEM",
        "LASTNEW.ATTR(0fabc)",
        f"SYSMESSAGE {marker} C|attr_set|[done]",
        f"SYSMESSAGE {marker} C|attr|[<LASTNEW.ATTR>]",
        f"SYSMESSAGE {marker} C_END",
    ]
    sections = "\n" + f"""[CHARDEF 0x0193]
DEFNAME={DWORD_HEX_HIGH_NAME}
NAME=synthetic DWORD hex high
ID=0x0193
ANIM=0FFC78C7F

[CHARDEF 0x0194]
DEFNAME={DWORD_HEX_LOW_NAME}
NAME=synthetic DWORD hex low
ID=0x0194
ANIM=03fbc7f

[ITEMDEF 0x0E7D]
DEFNAME=SYNTHETIC_DWORD_HEX_ITEM
NAME=synthetic DWORD hex item
TYPE=T_NORMAL
"""
    return "\n".join(login) + "\n", sections


def dotted_expression_scripts() -> tuple[str, str, str]:
    """Return the login lines, extra sections and @EnvironChange body."""

    marker = DOTTED_PROBE_MARKER
    login = [
        "TAG.probe_text=chartext",
        "TAG.probe_num=7",
        # 0.99 current-value TAG assignments must evaluate against the
        # existing numeric value while ordinary and string writes stay intact.
        "TAG.current_num=10",
        "SYSMESSAGE " + marker + " C|tag_current_initial|[<tag(current_num)>]",
        "TAG.current_num=#+1",
        "SYSMESSAGE " + marker + " C|tag_current_property_add|[<tag(current_num)>]",
        "TAG(current_num,#+3)",
        "SYSMESSAGE " + marker + " C|tag_current_add|[<tag(current_num)>]",
        "TAG(current_num,#-2)",
        "SYSMESSAGE " + marker + " C|tag_current_sub|[<tag(current_num)>]",
        "TAG.current_num=#+<EVAL 2>",
        "SYSMESSAGE " + marker + " C|tag_current_expression_add|[<tag(current_num)>]",
        "TAG.current_num=42",
        "SYSMESSAGE " + marker + " C|tag_current_absolute|[<tag(current_num)>]",
        "TAG(current_text,seed)",
        "SYSMESSAGE " + marker + " C|tag_current_string|[<tag(current_text)>]",
        "TAG.script_hash=#0DE97",
        "SYSMESSAGE " + marker + " C|tag_script_hash_literal|[<tag(script_hash)>]",
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
        # Statements written as calls, NAME(args) and REF.NAME(args), with
        # arguments that hold spaces and <...> expressions, and a statement
        # key that holds an expression.
        "VAR probe_call_count,0",
        "VAR probe_call_log,start",
        "F_DOTTED_CALL(3,4)",
        "F_DOTTED_CALL(5, 6)",
        "F_DOTTED_CALL(<src.str>)",
        "F_DOTTED_CALL(<eval 1+2>)",
        "SYSMESSAGE " + marker + " C|call_count|[<VAR(probe_call_count)>]",
        "SYSMESSAGE " + marker + " C|call_log|[<VAR(probe_call_log)>]",
        "SYSMESSAGE(" + marker + " C|builtin_call|[reached])",
        "TAG(probe_call_tag,9)",
        "SRC.TAG(probe_src_call_tag,8)",
        "VAR(probe_var_call,7)",
        "FINDUID(<src.serial>).TAG.probe_key_escape=12",
        "SYSMESSAGE " + marker + " C|call_readback|[<tag(probe_call_tag)>|<tag(probe_src_call_tag)>|"
        "<var(probe_var_call)>|<tag(probe_key_escape)>]",
        # Loops that reach the iteration limit, each run twice: the limit is
        # logged once per loop.
        "F_DOTTED_CAPPED_WHILE",
        "F_DOTTED_CAPPED_WHILE",
        "F_DOTTED_CAPPED_FOR",
        "F_DOTTED_CAPPED_FOR",
        "SYSMESSAGE " + marker + " C|capped_loops_returned|[yes]",
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
    login += ["VAR dotted_bare_calls,0"]
    for key, condition in DOTTED_CONDITION_ROWS:
        login += [
            f"IF {condition}",
            f"SYSMESSAGE {marker} C|{key}|[1]",
            "ELSE",
            f"SYSMESSAGE {marker} C|{key}|[0]",
            "ENDIF",
        ]
    login += [
        "SYSMESSAGE " + marker + " C|cond_bare_function_count|[<VAR(dotted_bare_calls)>]",
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
        "\n[SPELL 4]\n"
        "DEFNAME=s_fixture_heal\n"
        "NAME=synthetic fixture heal\n"
        "RUNES=IM\n"
        "MANAUSE=8\n"
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
        "i_dotted_probe 0x0E7B\n"
        "str 0\n"
        "\n[FUNCTION f_dotted_serial]\n"
        "RETURN <SERIAL>\n"
        "\n[FUNCTION f_dotted_bare_probe]\n"
        "VAR dotted_bare_calls,<EVAL <VAR(dotted_bare_calls)>+1>\n"
        "RETURN 1\n"
        "\n[FUNCTION f_dotted_arg]\n"
        "RETURN <ARGS>\n"
        "\n[FUNCTION f_dotted_disposable]\n"
        "RETURN <VAR(dotted_disposable)>\n"
        "\n[FUNCTION f_dotted_call]\n"
        "VAR probe_call_count,<EVAL <VAR(probe_call_count)>+1>\n"
        "VAR probe_call_log,<VAR(probe_call_log)>[<ARGS>]\n"
        "RETURN 1\n"
        "\n[FUNCTION f_dotted_capped_while]\n"
        f"{DOTTED_PROBE_CAPPED_WHILE}\n"
        "ENDWHILE\n"
        "\n[FUNCTION f_dotted_capped_for]\n"
        f"{DOTTED_PROBE_CAPPED_FOR}\n"
        "ENDFOR\n"
    )
    return "\n".join(login) + "\n", sections, environ_change
def timer_sibling_mutation_definitions(*, owner_first: bool = False) -> str:
    """Return three independent A->B sibling-mutation scenarios.

    Case 1 deletes B while the owner is removing A.  Case 2 reparents B to a
    live destination while the remaining C subtree is cleaned.  Case 3 does
    the same with a nested B subtree and a nested C subtree.  The callbacks
    emit only generic synthetic markers; the test checks the object registry
    and parent links after the deferred collector has run.
    """

    owner_uids = MUTATION_OWNER_SERIALS
    dest_uids = tuple(UID_F_ITEM | serial for serial in MUTATION_DEST_SERIALS)
    b_uids = tuple(UID_F_ITEM | serial for serial in MUTATION_B_SERIALS)
    uid_tokens = [
        *(str(serial) for serial in owner_uids),
        *(str(uid) for uid in dest_uids),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_KEEP_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_A_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_B_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_C_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_C_CHILD_SERIALS),
    ]
    uid_checks = "|".join(f"<ISUIDVALID {uid}>" for uid in uid_tokens)

    def action_item(
        item_id: int,
        defname: str,
        timer_marker: str,
        callback_marker: str,
        action_lines: str,
        action_return_marker: str,
        owner_return_marker: str,
    ) -> str:
        timer_action = "CONT.REMOVE\n" if owner_first else "REMOVE\n"
        return (
            f"\n[ITEMDEF 0x{item_id:04X}]\n"
            f"DEFNAME={defname}\n"
            f"NAME={defname.lower()}\n"
            "TYPE=T_EQ_SCRIPT\n"
            "LAYER=30\n"
            "ON=@Timer\n"
            f"SERV.B {timer_marker}\n"
            f"{timer_action}"
            f"SERV.B {timer_marker}_RETURNED\n"
            "RETURN 1\n"
            "ON=@UnEquip\n"
            f"SERV.B {callback_marker}\n"
            f"{action_lines}"
            f"SERV.B {action_return_marker}\n"
            "CONT.REMOVE\n"
            f"SERV.B {owner_return_marker}\n"
            "RETURN 1\n"
        )

    case1 = action_item(
        0x0E7B,
        "SYNTHETIC_MUTATION_A_DELETE",
        "SPHERE_MUT_CASE1_TIMER",
        "SPHERE_MUT_CASE1_CALLBACK",
        f"FINDUID({b_uids[0]}).REMOVE\n",
        "SPHERE_MUT_CASE1_B_REMOVE_RETURNED",
        "SPHERE_MUT_CASE1_OWNER_REMOVE_RETURNED",
    )
    case2 = action_item(
        0x0E7F,
        "SYNTHETIC_MUTATION_A_REPARENT",
        "SPHERE_MUT_CASE2_TIMER",
        "SPHERE_MUT_CASE2_CALLBACK",
        f"FINDUID({b_uids[1]}).CONT={dest_uids[1]}\n",
        "SPHERE_MUT_CASE2_B_REPARENT_RETURNED",
        "SPHERE_MUT_CASE2_OWNER_REMOVE_RETURNED",
    )
    case3 = action_item(
        0x0E83,
        "SYNTHETIC_MUTATION_A_NESTED_REPARENT",
        "SPHERE_MUT_CASE3_TIMER",
        "SPHERE_MUT_CASE3_CALLBACK",
        f"FINDUID({b_uids[2]}).CONT={dest_uids[2]}\n",
        "SPHERE_MUT_CASE3_B_REPARENT_RETURNED",
        "SPHERE_MUT_CASE3_OWNER_REMOVE_RETURNED",
    )

    def relation_item(
        item_id: int,
        defname: str,
        parent_marker: str,
    ) -> str:
        return (
            f"\n[ITEMDEF 0x{item_id:04X}]\n"
            f"DEFNAME={defname}\n"
            f"NAME={defname.lower()}\n"
            "TYPE=T_EQ_SCRIPT\n"
            "LAYER=30\n"
            + "ON=@Timer\n"
            f"SERV.B {parent_marker} <CONT.SERIAL>\n"
            "RETURN 1\n"
        )

    return (
        "\n[ITEMDEF 0x0E7A]\n"
        "DEFNAME=SYNTHETIC_MUTATION_OBSERVER\n"
        "NAME=synthetic mutation observer\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        f"ON=@Equip\nTIMER={MUTATION_OBSERVER_DELAY_SECONDS}\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_MUTATION_LISTENER_ALIVE\n"
        "SERV.SAVE 1\n"
        f"SERV.B SPHERE_MUTATION_UIDS_AFTER {uid_checks}\n"
        f"SERV.B SPHERE_MUT_CASE2_B_PARENT <FINDUID({b_uids[1]}).CONT.SERIAL>\n"
        f"SERV.B SPHERE_MUT_CASE3_B_PARENT <FINDUID({b_uids[2]}).CONT.SERIAL>\n"
        f"SERV.B SPHERE_MUT_CASE1_KEEP_PARENT <FINDUID({UID_F_ITEM | MUTATION_KEEP_SERIALS[0]}).CONT.SERIAL>\n"
        f"SERV.B SPHERE_MUT_CASE2_KEEP_PARENT <FINDUID({UID_F_ITEM | MUTATION_KEEP_SERIALS[1]}).CONT.SERIAL>\n"
        f"SERV.B SPHERE_MUT_CASE3_KEEP_PARENT <FINDUID({UID_F_ITEM | MUTATION_KEEP_SERIALS[2]}).CONT.SERIAL>\n"
        "RETURN 0\n"
        + case1
        + "\n[ITEMDEF 0x0E7C]\n"
        "DEFNAME=SYNTHETIC_MUTATION_B_DELETE\n"
        "NAME=synthetic mutation B delete\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        + "\n[ITEMDEF 0x0E7D]\n"
        "DEFNAME=SYNTHETIC_MUTATION_C_DELETE\n"
        "NAME=synthetic mutation C delete\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + "\n[ITEMDEF 0x0E7E]\n"
        "DEFNAME=SYNTHETIC_MUTATION_NESTED_1\n"
        "NAME=synthetic mutation nested one\n"
        "TYPE=T_NORMAL\n"
        + case2
        + relation_item(
            0x0E80,
            "SYNTHETIC_MUTATION_B_REPARENT",
            "SPHERE_MUT_CASE2_B_PARENT",
        )
        + "\n[ITEMDEF 0x0E81]\n"
        "DEFNAME=SYNTHETIC_MUTATION_C_REPARENT\n"
        "NAME=synthetic mutation C reparent\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + "\n[ITEMDEF 0x0E82]\n"
        "DEFNAME=SYNTHETIC_MUTATION_NESTED_2\n"
        "NAME=synthetic mutation nested two\n"
        "TYPE=T_NORMAL\n"
        + case3
        + relation_item(
            0x0E84,
            "SYNTHETIC_MUTATION_B_NESTED",
            "SPHERE_MUT_CASE3_B_PARENT",
        )
        + "\n[ITEMDEF 0x0E86]\n"
        "DEFNAME=SYNTHETIC_MUTATION_C_NESTED\n"
        "NAME=synthetic mutation C nested\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + "\n[ITEMDEF 0x0E87]\n"
        "DEFNAME=SYNTHETIC_MUTATION_NESTED_3\n"
        "NAME=synthetic mutation nested three\n"
        "TYPE=T_NORMAL\n"
        + "\n[ITEMDEF 0x0E88]\n"
        "DEFNAME=SYNTHETIC_MUTATION_DEST_1\n"
        "NAME=synthetic mutation destination one\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + "\n[ITEMDEF 0x0E89]\n"
        "DEFNAME=SYNTHETIC_MUTATION_DEST_2\n"
        "NAME=synthetic mutation destination two\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + "\n[ITEMDEF 0x0E8A]\n"
        "DEFNAME=SYNTHETIC_MUTATION_DEST_3\n"
        "NAME=synthetic mutation destination three\n"
        "TYPE=T_CONTAINER\n"
        "TDATA2=1\n"
        + relation_item(0x0E8B, "SYNTHETIC_MUTATION_KEEP_1", "SPHERE_MUT_CASE1_KEEP_PARENT")
        + relation_item(0x0E8C, "SYNTHETIC_MUTATION_KEEP_2", "SPHERE_MUT_CASE2_KEEP_PARENT")
        + relation_item(0x0E8D, "SYNTHETIC_MUTATION_KEEP_3", "SPHERE_MUT_CASE3_KEEP_PARENT")
    )


def ontick_content_mutation_definitions() -> str:
    """Return an equipped timer callback that mutates its owner's list."""

    victim_uid = ONTICK_VICTIM_SERIAL
    sibling_uid = UID_F_ITEM | ONTICK_SIBLING_SERIAL
    return (
        "\n[ITEMDEF 0x0EA0]\n"
        "DEFNAME=SYNTHETIC_ONTICK_MUTATOR\n"
        "NAME=synthetic OnTick mutator\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_ONTICK_MUTATOR_TIMER\n"
        f"FINDUID({victim_uid}).REMOVE\n"
        f"FINDUID({sibling_uid}).REMOVE\n"
        "SERV.B SPHERE_ONTICK_MUTATOR_RETURNED\n"
        "RETURN 1\n"
        "ON=@UnEquip\n"
        "SERV.B SPHERE_ONTICK_MUTATOR_UNEQUIP\n"
        "RETURN 1\n"
        "\n[ITEMDEF 0x0EA1]\n"
        "DEFNAME=SYNTHETIC_ONTICK_SIBLING\n"
        "NAME=synthetic OnTick sibling\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@UnEquip\n"
        "SERV.B SPHERE_ONTICK_SIBLING_UNEQUIP\n"
        "RETURN 1\n"
        "\n[ITEMDEF 0x0EA2]\n"
        "DEFNAME=SYNTHETIC_ONTICK_LISTENER\n"
        "NAME=synthetic OnTick listener\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        f"ON=@Equip\nTIMER={ONTICK_LISTENER_TIMER_SECONDS}\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_ONTICK_LISTENER_ALIVE\n"
        "SERV.B SPHERE_ONTICK_UIDS_AFTER "
        f"<ISUIDVALID {ONTICK_OWNER_SERIAL}>|"
        f"<ISUIDVALID {ONTICK_VICTIM_SERIAL}>|"
        f"<ISUIDVALID {UID_F_ITEM | ONTICK_MUTATOR_SERIAL}>|"
        f"<ISUIDVALID {sibling_uid}>|"
        f"<ISUIDVALID {UID_F_ITEM | ONTICK_LISTENER_SERIAL}>\n"
        "RETURN 1\n"
    )


def container_shutdown_definitions() -> str:
    """Return an event-backed nested-container teardown reproducer."""

    destination_uid = UID_F_ITEM | SHUTDOWN_DEST_SERIAL
    insert_uid = UID_F_ITEM | SHUTDOWN_INSERT_SERIAL
    pack_uid = UID_F_ITEM | SHUTDOWN_RUNTIME_PACK_SERIAL
    return (
        "\n[ITEMDEF 0x0E7B]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_TRIGGER\n"
        "NAME=synthetic shutdown trigger\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_SHUTDOWN_TIMER\n"
        f"FINDUID({UID_F_ITEM | SHUTDOWN_RUNTIME_PACK_SERIAL}).CONT={destination_uid}\n"
        f"SERV.B SPHERE_SHUTDOWN_FINAL_PACK <FINDUID({pack_uid}).CONT.SERIAL>\n"
        f"SERV.B SPHERE_SHUTDOWN_FINAL_INSERT <FINDUID({insert_uid}).CONT.SERIAL>\n"
        "CONT.REMOVE\n"
        "RETURN 1\n"
        "\n[ITEMDEF 0x0E7D]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_PACK\n"
        "NAME=synthetic shutdown pack\n"
        "TYPE=CONTAINER\n"
        "TDATA2=1\n"
        "\n[ITEMDEF 0x0E8F]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_RUNTIME_PACK\n"
        "NAME=synthetic shutdown runtime pack\n"
        "TYPE=T_EQ_SCRIPT\n"
        "ON=@UnEquip\n"
        "SERV.B SPHERE_SHUTDOWN_RUNTIME_EVENT\n"
        f"FINDUID({insert_uid}).CONT={destination_uid}\n"
        f"CONT={destination_uid}\n"
        "SERV.B SPHERE_SHUTDOWN_RUNTIME_EVENT_MOVED <CONT.SERIAL>\n"
        "RETURN 1\n"
        "\n[ITEMDEF 0x0E7E]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_CHILD\n"
        "NAME=synthetic shutdown child\n"
        "TYPE=T_NORMAL\n"
        "\n[ITEMDEF 0x0E90]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_INSERT\n"
        "NAME=synthetic shutdown hook insert\n"
        "TYPE=T_NORMAL\n"
        "\n[ITEMDEF 0x0E88]\n"
        "DEFNAME=SYNTHETIC_SHUTDOWN_DEST\n"
        "NAME=synthetic shutdown destination\n"
        "TYPE=CONTAINER\n"
        "TDATA2=1\n"
        f"\n[TYPEDEF {SHUTDOWN_EVENT_NAME}]\n"
        "ON=@UnEquip\n"
        "SERV.B SPHERE_SHUTDOWN_EVENT\n"
        f"FINDUID({insert_uid}).CONT={destination_uid}\n"
        f"CONT={destination_uid}\n"
        "SERV.B SPHERE_SHUTDOWN_EVENT_MOVED <CONT.SERIAL>\n"
        "RETURN 1\n"
    )


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
    named_item_name_probe: bool = False,
    named_resource_id_probe: bool = False,
    timer_lifetime_probe: bool = False,
    memory_timer_probe: bool = False,
    dotted_expression_probe: bool = False,
    format_compat_probe: bool = False,
    arg_locals_probe: bool = False,
    findarg_probe: bool = False,
    timer_lifetime_item_first_probe: bool = False,
    timer_sibling_mutation_probe: bool = False,
    timer_sibling_mutation_owner_first_probe: bool = False,
    ontick_content_mutation_probe: bool = False,
    container_shutdown_probe: bool = False,
    book_pages_probe: bool = False,
    dialog_button_probe: bool = False,
    dialog_argo_layout_probe: bool = False,
    dialog_flow_layout_probe: bool = False,
    dword_hex_probe: bool = False,
    region_weather_probe: bool = False,
    suppress_login_item: bool = False,
    spawn_gem_probe: bool = False,
    spawn_point_probe: bool = False,
    metadata_roundtrip_probe: bool = False,
    escape_overflow_probe: bool = False,
    movement_stairs_probe: bool = False,
    character_content_probe: bool = False,
) -> None:
    timer_lifetime_probe = timer_lifetime_probe or timer_lifetime_item_first_probe
    book_pages_probe_sections = book_pages_sections() if book_pages_probe else ""
    unknown_newbie_section = (
        "\n[NEWBIE SYNTHETIC_UNKNOWN_SKILL]\nITEMNEWBIE=0x0E72\n"
        if unknown_newbie
        else ""
    )
    dotted_expression_login, dotted_expression_sections, environ_change_body = (
        dotted_expression_scripts() if dotted_expression_probe else ("", "", "RETURN\n")
    )
    arg_locals_login, arg_locals_sections = (
        arg_locals_scripts() if arg_locals_probe else ("", "")
    )
    findarg_login, findarg_sections = (
        findarg_scripts() if findarg_probe else ("", "")
    )
    dialog_button_login, dialog_button_sections = (
        dialog_button_scripts(
            argo_layout=dialog_argo_layout_probe, flow_layout=dialog_flow_layout_probe
        )
        if dialog_button_probe or dialog_argo_layout_probe or dialog_flow_layout_probe
        else ("", "")
    )
    dword_hex_login, dword_hex_sections = (
        dword_hex_scripts() if dword_hex_probe else ("", "")
    )
    region_weather_login = (
        f"SYSMESSAGE {REGION_WEATHER_MARKER} "
        "<SRC.SECTOR.RAINCHANCE>|<SRC.SECTOR.COLDCHANCE>\n"
        f"SYSMESSAGE {REGION_WEATHER_MARKER}_END\n"
        if region_weather_probe
        else ""
    )
    character_content_login = (
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_UID "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT).SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_PARENT "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT).CONT.SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_LAYERED_UID "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT_LAYERED).SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_LAYERED_PARENT "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT_LAYERED).CONT.SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_END\n"
        if character_content_probe
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
    world_save_probe_script = (
        "SERV.SAVE\n"
        if world_save_probe
        else ""
    )
    world_save_logout_event_login = (
        "EVENTS=e_WorldSaveProbe\n"
        if world_save_probe
        else ""
    )
    world_save_logout_event_sections = (
        "\n[EVENTS e_WorldSaveProbe]\nON=@Logout\nSERV.SAVE\nRETURN 0\n"
        if world_save_probe
        else ""
    )
    world_save_login_probe_script = (
        world_save_probe_script
        if world_save_probe and suppress_login_item
        else ""
    )
    escape_overflow_login = ""
    if escape_overflow_probe:
        # Keep the source line within SCRIPT_MAX_LINE_LEN while making the
        # resolved name materially longer than its <NAME> escape tag.
        escape_line = (
            "VAR overflow_sink,"
            + ("P" * 3000)
            + "<NAME>"
            + ("S" * 1050)
        )
        assert len(escape_line) < 4096
        escape_overflow_login = (
            escape_line
            + "\n"
            + f"SYSMESSAGE {ESCAPE_OVERFLOW_MARKER}\n"
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
    memory_timer_itemdef = (
        "\n[TYPEDEF 74]\n"
        "DEFNAME=T_EQ_MEMORY_OBJ\n"
        "\n[ITEMDEF 0x2007]\n"
        "DEFNAME=i_memory\n"
        "TYPE=T_EQ_MEMORY_OBJ\n"
        "LAYER=30\n"
        f"\n[ITEMDEF 0x{MEMORY_TIMER_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_MEMORY_TIMER\n"
        "NAME=created memory\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Timer\n"
        f"SERV.B {MEMORY_TIMER_MARKER} <ISUIDVALID {MEMORY_TIMER_ITEM_UID}>\n"
        "REMOVE\n"
        f"SERV.B {MEMORY_TIMER_REMOVED_MARKER} <ISUIDVALID {MEMORY_TIMER_ITEM_UID}>\n"
        "RETURN 1\n"
        if memory_timer_probe
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
    mutation_uid_tokens = [
        *(str(serial) for serial in MUTATION_OWNER_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_DEST_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_KEEP_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_A_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_B_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_C_SERIALS),
        *(str(UID_F_ITEM | serial) for serial in MUTATION_C_CHILD_SERIALS),
    ]
    mutation_uid_checks = "|".join(
        f"<ISUIDVALID {uid}>" for uid in mutation_uid_tokens
    )
    timer_sibling_mutation_before_markers = (
        f"SERV.B SPHERE_MUTATION_UIDS_BEFORE {mutation_uid_checks}\n"
        if timer_sibling_mutation_probe or timer_sibling_mutation_owner_first_probe
        else ""
    )
    timer_sibling_mutation_observer_login = (
        "NEWITEM SYNTHETIC_MUTATION_OBSERVER\n"
        "EQUIPLAST\n"
        if timer_sibling_mutation_probe or timer_sibling_mutation_owner_first_probe
        else ""
    )
    timer_sibling_mutation_sections = (
        timer_sibling_mutation_definitions(
            owner_first=timer_sibling_mutation_owner_first_probe
        )
        if timer_sibling_mutation_probe or timer_sibling_mutation_owner_first_probe
        else ""
    )
    ontick_content_mutation_sections = (
        ontick_content_mutation_definitions()
        if ontick_content_mutation_probe
        else ""
    )
    container_shutdown_sections = (
        container_shutdown_definitions() if container_shutdown_probe else ""
    )
    container_shutdown_login = (
        f"FINDUID({UID_F_ITEM | SHUTDOWN_PACK_SERIAL}).EVENTS={SHUTDOWN_EVENT_NAME}\n"
        if container_shutdown_probe
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
    spawn_gem_itemdef = (
        f"\n[ITEMDEF 0x{SPAWN_GEM_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_SPAWN_GEM\n"
        "NAME=synthetic spawn gem\n"
        "TYPE=T_SPAWN_ITEM\n"
        if spawn_gem_probe
        else ""
    )
    spawn_point_itemdef = (
        f"\n[ITEMDEF 0x{SPAWN_POINT_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_SPAWN_POINT\n"
        "NAME=synthetic spawn point\n"
        "TYPE=T_SPAWN_ITEM\n"
        if spawn_point_probe
        else ""
    )
    spawn_point_product_itemdef = (
        f"\n[ITEMDEF 0x{SPAWN_POINT_PRODUCT_ID:04X}]\n"
        f"DEFNAME={SPAWN_POINT_PRODUCT_NAME}\n"
        "NAME=synthetic spawn product\n"
        "TYPE=T_NORMAL\n"
        "ON=@Create\n"
        f"SERV.B {SPAWN_POINT_MARKER}\n"
        if spawn_point_probe
        else ""
    )
    movement_stairs_itemdefs = (
        f"\n[ITEMDEF 0x{MOVEMENT_STAIRS_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_MOVEMENT_STAIRS\n"
        "NAME=synthetic movement stairs\n"
        "TYPE=T_NORMAL\n"
        "\n[CHARDEF SYNTHETIC_MOVEMENT_CHAR]\n"
        "DEFNAME=SYNTHETIC_MOVEMENT_CHAR\n"
        "NAME=synthetic movement human\n"
        "ID=0x0190\n"
        "CAN=0x14\n"
        "STR=100\n"
        "DEX=100\n"
        if movement_stairs_probe
        else ""
    )
    spawn_point_login_event = (
        f"\n[EVENTS SYNTHETIC_SPAWN_LOGIN]\n"
        "ON=@LogIn\n"
        f"FINDUID({SPAWN_POINT_SERIAL}).TIMER=1\n"
        if spawn_point_probe
        else ""
    )
    timer_remove = "REMOVE" if timer_lifetime_item_first_probe else "CONT.REMOVE"
    unequip_remove = "CONT.REMOVE" if timer_lifetime_item_first_probe else "REMOVE"
    typedef_container_table = (
        "\n[TYPEDEFS]\nT_NORMAL 0\nT_CONTAINER 1\n"
        + ("T_SPAWN_ITEM 69\n" if spawn_gem_probe or spawn_point_probe else "")
        if (
            typedef_container_probe
            or timer_lifetime_probe
            or timer_sibling_mutation_probe
            or timer_sibling_mutation_owner_first_probe
            or container_shutdown_probe
            or format_compat_probe
            or spawn_gem_probe
            or spawn_point_probe
        )
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
    multi_property_probe = (
        multi_property_probe or named_item_name_probe or format_compat_probe
    )
    multi_property_typedef = (
        "\n[TYPEDEF 47]\nDEFNAME=T_MULTI\n"
        if multi_property_probe
        else ""
    )
    map_property_typedef = (
        "\n[TYPEDEF 73]\nDEFNAME=T_MAP\n"
        if format_compat_probe
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
    map_property_itemdef = (
        "\n[ITEMDEF 0x4001]\n"
        "DEFNAME=SYNTHETIC_MAP\n"
        "NAME=synthetic map\n"
        "TYPE=T_MAP\n"
        if format_compat_probe
        else ""
    )
    metadata_roundtrip_itemdef = (
        f"\n[ITEMDEF 0x{ROUNDTRIP_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_ROUNDTRIP_ITEM\n"
        "NAME=synthetic round-trip item\n"
        "TYPE=T_NORMAL\n"
        if metadata_roundtrip_probe
        else ""
    )
    # An @Timer handler that falls through lets the default timer path log the
    # item's name instead of silently deleting the item.
    named_item_name_sections = (
        f"\n[ITEMDEF 0x{NAMED_TIMER_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_NAMED_TIMER\n"
        "NAME=synthetic timer item\n"
        "TYPE=T_NORMAL\n"
        "ON=@Timer\n"
        "SERV.B SPHERE_NAMED_TIMER_TICK\n"
        if named_item_name_probe
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
    character_content_itemdef = (
        f"\n[ITEMDEF 0x{CHARACTER_CONTENT_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_CHARACTER_CONTENT\n"
        "NAME=synthetic character content\n"
        "TYPE=T_NORMAL\n"
        f"\n[ITEMDEF 0x{CHARACTER_CONTENT_LAYERED_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_CHARACTER_CONTENT_LAYERED\n"
        "NAME=synthetic character content with default layer\n"
        "LAYER=29\n"
        "TYPE=T_NORMAL\n"
        if character_content_probe
        else ""
    )
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

[TYPEDEF 177]
DEFNAME=t_fixture_events

[PROFESSION 11]
DEFNAME=class_fixture

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

""" + timer_lifetime_probe_itemdefs + memory_timer_itemdef + character_content_itemdef + """
[ITEMDEF 0x09B2]
DEFNAME=SYNTHETIC_SHIRT
NAME=synthetic shirt
TYPE=T_NORMAL

""" + default_char_definition + metadata_roundtrip_itemdef + """
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
; ClientDetach removes e_AllPlayers before dispatching @Logout. Keep the
; synthetic character's logout trigger handled so unknown-keyword probes only
; report the keywords they intentionally exercise.
ON=@Logout
RETURN 0

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
""" + world_save_logout_event_login + escape_overflow_login + world_save_login_probe_script + container_shutdown_login + findarg_login + timer_lifetime_baseline + timer_sibling_mutation_before_markers + """
""" + timer_lifetime_observer_login + timer_sibling_mutation_observer_login + """
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
""" + character_content_login + ("" if timer_lifetime_probe or memory_timer_probe or suppress_login_item or character_content_probe else "NEWITEM SYNTHETIC_HAIR\n") + """
""" + world_load_counts_probe_script + unknown_keyword_probe_script + unknown_keyword_overflow_script + dotted_expression_login + arg_locals_login + dword_hex_login + region_weather_login + dialog_button_login + typedef_container_itemdef + multi_property_typedef + map_property_typedef + multi_property_itemdef + map_property_itemdef + """
ON=@EnvironChange
""" + environ_change_body + """ON=@Logout
""" + ("" if suppress_login_item else world_save_probe_script) + """
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
""" + dotted_expression_sections + arg_locals_sections + dword_hex_sections + dialog_button_sections + """
[SPEECH spk_AllPlayers]

[AREA Synthetic world]
P=128,128,0
RECT=1,1,6143,4096

""" + (
        f"[AREA Synthetic weather]\n"
        "P=128,128,0\n"
        "RECT=120,120,136,136\n"
        f"RAINCHANCE={REGION_WEATHER_RAIN}\n"
        f"COLDCHANCE={REGION_WEATHER_COLD}\n"
        if region_weather_probe
        else ""
    ) + """

""" + skill_sections(dword_hex_probe=dword_hex_probe) + timer_sibling_mutation_sections + ontick_content_mutation_sections + container_shutdown_sections + findarg_sections + world_save_logout_event_sections + """

[NEWBIE MAGERY]
ITEMNEWBIE=0x0E72

[NEWBIE resist]
ITEMNEWBIE=0x0E73
""" + unknown_newbie_section + named_resource_id_probe_sections + named_item_name_sections
        + spawn_point_login_event
        + spawn_gem_itemdef
        + spawn_point_itemdef
        + spawn_point_product_itemdef
        + movement_stairs_itemdefs
        + book_pages_probe_sections,
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


def write_escape_overflow_save(root: Path) -> None:
    """Write one existing account/character for the login escape probe."""

    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            (
                f"[{ESCAPE_OVERFLOW_ACCOUNT}]",
                f"PASSWORD={ESCAPE_OVERFLOW_PASSWORD}",
                "CHARUID=1",
                "LASTCHARUID=1",
                "[EOF]",
            )
        ),
    )
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")
    write_text(root / "save" / "sphereworld.scp", "[EOF]")
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            (
                "TITLE=Sphere synthetic login escape fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=1",
                f"ACCOUNT={ESCAPE_OVERFLOW_ACCOUNT}",
                f"NAME={ESCAPE_OVERFLOW_NAME}",
                "EVENTS=e_AllPlayers",
                "STR=100",
                "DEX=100",
                "INT=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            )
        ),
    )


def write_world_load_counts_save(
    root: Path,
    *,
    truncate_world_item: bool,
    unresolved_worldchar_type: bool,
    noncontainer_reference: bool,
    typedef_container_reference: bool,
    multi_property_reference: bool,
    named_item_names: bool,
    named_container_reference: bool,
    rejected_property: bool,
    weird_item: bool,
    child_before_parent: bool,
    format_compat_probe: bool,
    metadata_roundtrip_probe: bool,
    character_content_probe: bool,
) -> None:
    """Write a synthetic save with one selected world-load scenario."""

    world_sections = [
        "TITLE=Sphere synthetic object-count fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    if character_content_probe:
        # The item is written with CONT=<character> and no LAYER key.  Stock
        # 0.99 preserves this direct character relation for non-equippable
        # items.
        world_sections.extend([])
    elif metadata_roundtrip_probe:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_ROUNDTRIP_ITEM]",
                "SERIAL=4",
                "P=128,128,0",
                f"DISPID={ROUNDTRIP_DISP_ID:04x}",
                'Tag.roundtrip="value with trailing space "',
                'Tag.empty=""',
                "Tag.numeric=42",
                "Tag.hash_literal=#0DE97",
                "Tag.hash_expression=#<EVAL 2>",
            ]
        )
    elif format_compat_probe:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_MULTI]",
                "SERIAL=6",
                "P=128,128,0",
                "LEGACY_UNKNOWN=preserve-me",
                "REGION.FLAGS=0d2",
                "[WORLDITEM SYNTHETIC_MAP]",
                "SERIAL=7",
                "P=129,128,0",
                "PIN=100,200,5",
                "PIN=300,400,6",
            ]
        )
    elif child_before_parent:
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_OBJECT]",
                "SERIAL=5",
                "CONT=4",
                "LEGACY_UNKNOWN=preserve-me",
                "REGION.FLAGS=0d2",
                "[WORLDITEM DEFAULTITEM]",
                "SERIAL=4",
                "P=128,128,0",
            ]
        )
    elif rejected_property:
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
    elif named_item_names:
        # Individually named items whose names are read while the save loads
        # (multi region realization) and when the saved timer expires.
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_NAMED_TIMER]",
                "SERIAL=4",
                f"NAME={NAMED_TIMER_ITEM_NAME}",
                "TIMER=1",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_MULTI]",
                "SERIAL=5",
                f"NAME={NAMED_MULTI_NAME}",
                "P=128,128,0",
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
    if character_content_probe:
        write_text(
            root / "accounts" / "sphereaccu.scp",
            "\n".join(
                [
                    f"[ACCOUNT {CHARACTER_CONTENT_ACCOUNT}]",
                    f"PASSWORD={CHARACTER_CONTENT_PASSWORD}",
                    f"LASTCHARUID={CHARACTER_CONTENT_CHAR_SERIAL}",
                    f"CHARUID={CHARACTER_CONTENT_CHAR_SERIAL}",
                    "[EOF]",
                ]
            ),
        )
        char_sections = [
            "[WORLDCHAR c_MAN]",
            f"SERIAL={CHARACTER_CONTENT_CHAR_SERIAL}",
            f"ACCOUNT={CHARACTER_CONTENT_ACCOUNT}",
            "EVENTS=e_AllPlayers",
            "STR=100",
            "INT=100",
            "DEX=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            "P=130,128,0",
            "[WORLDITEM SYNTHETIC_CHARACTER_CONTENT]",
            f"SERIAL={CHARACTER_CONTENT_ITEM_SERIAL}",
            f"CONT={CHARACTER_CONTENT_CHAR_SERIAL}",
            "[WORLDITEM SYNTHETIC_CHARACTER_CONTENT_LAYERED]",
            f"SERIAL={CHARACTER_CONTENT_LAYERED_ITEM_SERIAL}",
            f"CONT={CHARACTER_CONTENT_CHAR_SERIAL}",
            "[EOF]",
        ]
    elif rejected_property or child_before_parent or format_compat_probe or metadata_roundtrip_probe:
        write_text(
            root / "accounts" / "sphereaccu.scp",
            "\n".join(
                [
                    "[ACCOUNT FixturePlayer]",
                    "PASSWORD=fixture-pw",
                    "LASTCHARUID=4" if rejected_property else "LASTCHARUID=3",
                    "CHARUID=4" if rejected_property else "CHARUID=3",
                    "[EOF]",
                ]
            ),
        )
        if rejected_property:
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
        elif metadata_roundtrip_probe:
            char_sections = [
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                "ACCOUNT=FixturePlayer",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                'Tag.roundtrip="character trailing space "',
                "Tag.numeric=7",
                "P=130,128,0",
                "[EOF]",
            ]
        else:
            char_sections = [
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                "ACCOUNT=FixturePlayer",
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


def write_spawn_gem_save(root: Path, *, duplicate_serials: bool) -> None:
    """Seed top-level spawn gems, optionally repeating each saved serial."""

    world_sections = [
        "TITLE=Sphere synthetic spawn-gem serialization fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    for serial in SPAWN_GEM_SERIALS:
        repeats = 2 if duplicate_serials else 1
        for _ in range(repeats):
            world_sections.extend(
                [
                    "[WORLDITEM SYNTHETIC_SPAWN_GEM]",
                    f"SERIAL={UID_F_ITEM | serial}",
                    "TIMER=0",
                    'MORE1="SYNTHETIC_OBJECT"',
                    "MORE2=1",
                    "P=128,128,0",
                ]
            )
    world_sections.append("[EOF]")
    write_text(root / "save" / "sphereworld.scp", "\n".join(world_sections))
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic spawn-gem serialization fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            ]
        ),
    )


def write_spawn_point_save(root: Path) -> None:
    """Seed one timed spawn point whose target is a quoted resource name."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic spawn-point fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDITEM SYNTHETIC_SPAWN_POINT]",
                f"SERIAL={SPAWN_POINT_SERIAL}",
                "TIMER=60",
                f'MORE1="{SPAWN_POINT_PRODUCT_NAME}"',
                "MORE2=1",
                "MOREP=1,1,0",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic spawn-point fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                "ACCOUNT=FixturePlayer",
                "EVENTS=SYNTHETIC_SPAWN_LOGIN",
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
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                "[ACCOUNT FixturePlayer]",
                "PASSWORD=fixture-pw",
                "LASTCHARUID=3",
                "CHARUID=3",
                "[EOF]",
            ]
        ),
    )


def write_movement_stairs_save(root: Path) -> None:
    """Seed a climbable stair and an Admin probe player."""

    world_sections = [
        "TITLE=Sphere synthetic movement fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
        "[WORLDITEM SYNTHETIC_MOVEMENT_STAIRS]",
        f"SERIAL={MOVEMENT_STAIRS_SERIAL}",
        f"P={MOVEMENT_STAIRS_POINT[0]},{MOVEMENT_STAIRS_POINT[1]},{MOVEMENT_STAIRS_POINT[2]}",
        "[EOF]",
    ]
    write_text(root / "save" / "sphereworld.scp", "\n".join(world_sections))
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {MOVEMENT_STAIRS_ACCOUNT}]",
                f"PASSWORD={MOVEMENT_STAIRS_PASSWORD}",
                "PLEVEL=Admin",
                f"CHARUID={MOVEMENT_STAIRS_CHAR_SERIAL}",
                f"LASTCHARUID={MOVEMENT_STAIRS_CHAR_SERIAL}",
                "[EOF]",
            ]
        ),
    )
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic movement fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR SYNTHETIC_MOVEMENT_CHAR]",
                f"SERIAL={MOVEMENT_STAIRS_CHAR_SERIAL}",
                f"ACCOUNT={MOVEMENT_STAIRS_ACCOUNT}",
                "NAME=MovementProbeCharacter",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )


def write_dword_hex_save(root: Path) -> None:
    """Seed an existing account/character whose AGE uses Sphere hexadecimal."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic DWORD hex fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {DWORD_HEX_ACCOUNT}]",
                "PASSWORD=dword-hex-pw",
                "LASTCHARUID=3",
                "CHARUID=3",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic DWORD hex fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={DWORD_HEX_ACCOUNT}",
                "EVENTS=e_AllPlayers",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                f"AGE=0{DWORD_HEX_AGE:x}",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )


def write_region_weather_save(root: Path) -> None:
    """Seed an existing character inside the synthetic weather region."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic region weather fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {REGION_WEATHER_ACCOUNT}]",
                "PASSWORD=region-pw",
                "LASTCHARUID=3",
                "CHARUID=3",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic region weather fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={REGION_WEATHER_ACCOUNT}",
                "EVENTS=e_AllPlayers",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[EOF]",
            ]
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


def write_memory_timer_save(root: Path) -> None:
    """Seed a production-shaped i_memory script item with an active timer."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic memory timer fixture",
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
                "TITLE=Sphere synthetic memory timer fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={MEMORY_TIMER_OWNER_SERIAL}",
                "NPC=2",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=130,128,0",
                "[WORLDITEM SYNTHETIC_MEMORY_TIMER]",
                f"SERIAL={MEMORY_TIMER_ITEM_SERIAL}",
                f"CONT={MEMORY_TIMER_OWNER_SERIAL}",
                "LAYER=30",
                f"TIMER={MEMORY_TIMER_DELAY_SECONDS}",
                "[EOF]",
            ]
        ),
    )


def write_timer_sibling_mutation_save(root: Path) -> None:
    """Seed three independent sibling mutation cases and live destinations."""

    owner1, owner2, owner3 = MUTATION_OWNER_SERIALS
    dest1, dest2, dest3 = (UID_F_ITEM | serial for serial in MUTATION_DEST_SERIALS)
    keep1, keep2, keep3 = MUTATION_KEEP_SERIALS
    a1, a2, a3 = MUTATION_A_SERIALS
    b1, b2, b3 = MUTATION_B_SERIALS
    c1, c2, c3 = MUTATION_C_SERIALS
    c1_child, c2_child, c3_child = MUTATION_C_CHILD_SERIALS

    world_sections = [
        "TITLE=Sphere synthetic sibling mutation fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
        "[EOF]",
    ]
    write_text(root / "save" / "sphereworld.scp", "\n".join(world_sections))

    def char_header(serial: int, x: int) -> list[str]:
        return [
            "[WORLDCHAR c_MAN]",
            f"SERIAL={serial}",
            "NPC=2",
            "STR=100",
            "INT=100",
            "DEX=100",
            "HITS=100",
            "MAXHITS=100",
            "MANA=100",
            "STAM=100",
            f"P={x},140,0",
        ]

    chars = [
        *char_header(MUTATION_DESTINATION_OWNER_SERIAL, 180),
        "[WORLDITEM DEFAULTITEM]",
        "SERIAL=210",
        "LAYER=21",
        f"CONT={MUTATION_DESTINATION_OWNER_SERIAL}",
        "TIMERD=-1",
        "[WORLDITEM DEFAULTITEM]",
        "SERIAL=211",
        f"CONT={dest1}",
        "TIMERD=-1",
        "[WORLDITEM DEFAULTITEM]",
        "SERIAL=212",
        f"CONT={dest1}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_KEEP_1]",
        f"SERIAL={keep1}",
        f"CONT={dest1}",
        f"TIMER={MUTATION_KEEP_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_MUTATION_KEEP_2]",
        f"SERIAL={keep2}",
        f"CONT={dest2}",
        f"TIMER={MUTATION_KEEP_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_MUTATION_KEEP_3]",
        f"SERIAL={keep3}",
        f"CONT={dest3}",
        f"TIMER={MUTATION_KEEP_TIMER_SECONDS}",
        *char_header(owner1, 120),
        "[WORLDITEM DEFAULTITEM]",
        f"SERIAL={c1}",
        "LAYER=21",
        f"CONT={owner1}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_NESTED_1]",
        f"SERIAL={c1_child}",
        f"CONT={UID_F_ITEM | c1}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_B_DELETE]",
        f"SERIAL={b1}",
        f"CONT={owner1}",
        "LAYER=30",
        f"TIMER={MUTATION_RELATION_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_MUTATION_A_DELETE]",
        f"SERIAL={a1}",
        f"CONT={owner1}",
        "LAYER=30",
        f"TIMER={MUTATION_TIMER_SECONDS}",
        *char_header(owner2, 140),
        "[WORLDITEM DEFAULTITEM]",
        f"SERIAL={c2}",
        "LAYER=21",
        f"CONT={owner2}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_NESTED_2]",
        f"SERIAL={c2_child}",
        f"CONT={UID_F_ITEM | c2}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_B_REPARENT]",
        f"SERIAL={b2}",
        f"CONT={owner2}",
        "LAYER=30",
        f"TIMER={MUTATION_RELATION_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_MUTATION_A_REPARENT]",
        f"SERIAL={a2}",
        f"CONT={owner2}",
        "LAYER=30",
        f"TIMER={MUTATION_TIMER_SECONDS}",
        *char_header(owner3, 160),
        "[WORLDITEM DEFAULTITEM]",
        f"SERIAL={c3}",
        "LAYER=21",
        f"CONT={owner3}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_NESTED_3]",
        f"SERIAL={c3_child}",
        f"CONT={UID_F_ITEM | c3}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_MUTATION_B_NESTED]",
        f"SERIAL={b3}",
        f"CONT={owner3}",
        "LAYER=30",
        f"TIMER={MUTATION_RELATION_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_MUTATION_A_NESTED_REPARENT]",
        f"SERIAL={a3}",
        f"CONT={owner3}",
        "LAYER=30",
        f"TIMER={MUTATION_TIMER_SECONDS}",
        "[EOF]",
    ]
    write_text(root / "save" / "spherechars.scp", "\n".join(chars))



def write_container_shutdown_save(root: Path) -> None:
    """Seed nested reparent, hook-insert, and final-ownership cases."""

    chars = [
        "TITLE=Sphere synthetic container shutdown fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
        "[WORLDCHAR c_MAN]",
        "SERIAL=200",
        "NPC=2",
        "STR=100",
        "INT=100",
        "DEX=100",
        "HITS=100",
        "MAXHITS=100",
        "MANA=100",
        "STAM=100",
        "P=120,140,0",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_PACK]",
        f"SERIAL={SHUTDOWN_PACK_SERIAL}",
        "LAYER=21",
        "CONT=200",
        f"EVENTS={SHUTDOWN_EVENT_NAME}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_INSERT]",
        f"SERIAL={SHUTDOWN_INSERT_SERIAL}",
        f"CONT={UID_F_ITEM | SHUTDOWN_PACK_SERIAL}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_CHILD]",
        f"SERIAL={SHUTDOWN_CHILD_SERIAL}",
        f"CONT={UID_F_ITEM | SHUTDOWN_PACK_SERIAL}",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_RUNTIME_PACK]",
        f"SERIAL={SHUTDOWN_RUNTIME_PACK_SERIAL}",
        "LAYER=30",
        "CONT=200",
        "TIMERD=-1",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_TRIGGER]",
        "SERIAL=230",
        "LAYER=32",
        "CONT=200",
        "TIMER=1",
        "[WORLDCHAR c_MAN]",
        f"SERIAL={SHUTDOWN_DEST_OWNER_SERIAL}",
        "NPC=2",
        "STR=100",
        "INT=100",
        "DEX=100",
        "HITS=100",
        "MAXHITS=100",
        "MANA=100",
        "STAM=100",
        "P=180,140,0",
        "[WORLDITEM SYNTHETIC_SHUTDOWN_DEST]",
        f"SERIAL={SHUTDOWN_DEST_SERIAL}",
        "LAYER=21",
        f"CONT={SHUTDOWN_DEST_OWNER_SERIAL}",
        "TIMERD=-1",
        "[EOF]",
    ]
    write_text(root / "save" / "sphereworld.scp", "TITLE=Sphere synthetic container shutdown fixture\nVERSION=0.99\nSAVECOUNT=0\n[EOF]")
    write_text(root / "save" / "spherechars.scp", "\n".join(chars))


def write_ontick_content_mutation_save(root: Path) -> None:
    """Seed A before B so A's timer mutates the owner's live list."""

    owner = ONTICK_OWNER_SERIAL
    victim = ONTICK_VICTIM_SERIAL
    mutator = UID_F_ITEM | ONTICK_MUTATOR_SERIAL
    sibling = UID_F_ITEM | ONTICK_SIBLING_SERIAL
    listener = UID_F_ITEM | ONTICK_LISTENER_SERIAL
    chars = [
        "TITLE=Sphere synthetic OnTick content mutation fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
        "[WORLDCHAR c_MAN]",
        f"SERIAL={victim}",
        "NPC=2",
        "STR=100",
        "INT=100",
        "DEX=100",
        "HITS=100",
        "MAXHITS=100",
        "MANA=100",
        "STAM=100",
        "P=150,140,0",
        "[WORLDCHAR c_MAN]",
        f"SERIAL={owner}",
        "NPC=2",
        "STR=100",
        "INT=100",
        "DEX=100",
        "HITS=100",
        "MAXHITS=100",
        "MANA=100",
        "STAM=100",
        "P=160,140,0",
        # ContentAddPrivate inserts at the head while loading.  This file
        # order therefore realizes A, B, listener in the owner's list.
        "[WORLDITEM SYNTHETIC_ONTICK_LISTENER]",
        f"SERIAL={listener}",
        f"CONT={owner}",
        "LAYER=30",
        f"TIMER={ONTICK_LISTENER_TIMER_SECONDS}",
        "[WORLDITEM SYNTHETIC_ONTICK_SIBLING]",
        f"SERIAL={sibling}",
        f"CONT={owner}",
        "LAYER=30",
        "[WORLDITEM SYNTHETIC_ONTICK_MUTATOR]",
        f"SERIAL={mutator}",
        f"CONT={owner}",
        "LAYER=30",
        f"TIMER={ONTICK_MUTATOR_TIMER_SECONDS}",
        "[EOF]",
    ]
    write_text(root / "save" / "sphereworld.scp", "\n".join(
        [
            "TITLE=Sphere synthetic OnTick content mutation fixture",
            "VERSION=0.99",
            "SAVECOUNT=0",
            "[EOF]",
        ]
    ))
    write_text(root / "save" / "spherechars.scp", "\n".join(chars))


def write_events_attr_save(root: Path) -> None:
    """Seed one item and one NPC with current-format metadata."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic EVENTS/ATTR fixture",
                'VERSION="0.99z8"',
                "SAVECOUNT=0",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                f"SERIAL={EVENTS_ATTR_ITEM_SERIAL}",
                "P=128,128,0",
                *(f"EVENTS={value}" for value in EVENTS_ATTR_VALUES),
                f"CHANGER={EVENTS_ATTR_CHANGER}",
                f"ATTR=0x{EVENTS_ATTR_MASK:04X}",
                "LEGACY_UNKNOWN=keep-item",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic EVENTS/ATTR fixture",
                'VERSION="0.99z8"',
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={EVENTS_ATTR_CHAR_SERIAL}",
                "NPC=2",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=130,128,0",
                *(f"EVENTS={value}" for value in EVENTS_ATTR_VALUES),
                f"CHANGER={EVENTS_ATTR_CHANGER}",
                "LEGACY_UNKNOWN=keep-char",
                "[EOF]",
            ]
        ),
    )


def write_legacy_metadata_save(root: Path) -> None:
    """Seed long unquoted TAG text and 0.99 named ATTR keys."""

    sections = [
        "TITLE=Sphere synthetic legacy metadata fixture",
        'VERSION="0.99z8"',
        "SAVECOUNT=0",
    ]
    for serial, attr_key in zip(LEGACY_METADATA_ITEM_SERIALS, LEGACY_METADATA_ATTR_KEYS):
        sections.extend(
            [
                "[WORLDITEM DEFAULTITEM]",
                f"SERIAL={serial}",
                "P=128,128,0",
                f"TAG.{LEGACY_METADATA_TAG_KEY}={LEGACY_METADATA_TAG_VALUE}",
                f"{attr_key}=1",
            ]
        )
    sections.append("[EOF]")
    write_text(root / "save" / "sphereworld.scp", "\n".join(sections))
    write_text(
        root / "save" / "spherechars.scp",
        'TITLE="Sphere synthetic legacy metadata fixture"\nVERSION="0.99z8"\nSAVECOUNT=0\n[EOF]',
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path, help="directory to populate")
    parser.add_argument(
        "--mode",
        choices=tuple(sorted(FIXTURE_MODES)),
        help="select a complete named fixture recipe (legacy flags remain supported)",
    )
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
        "--character-content-probe",
        action="store_true",
        help="load a no-LAYER item whose CONT points directly at a character",
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
        "--roundtrip-integrity-probe",
        action="store_true",
        help="keep the login fixture focused on repeated save/load integrity",
    )
    parser.add_argument(
        "--metadata-roundtrip-probe",
        action="store_true",
        help="seed quoted TAG values and an explicit display id for save/reload checks",
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
        "--named-item-names",
        action="store_true",
        help="load a named IT_MULTI item and a named item whose saved timer expires",
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
        "--child-before-parent",
        action="store_true",
        help="write a contained item section before its saved container section",
    )
    parser.add_argument(
        "--format-compat-probe",
        action="store_true",
        help="write a multi REGION.* and map PIN round-trip fixture",
    )
    parser.add_argument(
        "--timer-lifetime-probe",
        action="store_true",
        help="seed a timer-owner, nested-item, sibling, and UID-cleanup probe",
    )
    parser.add_argument(
        "--memory-timer-probe",
        action="store_true",
        help="seed a production-shaped created-memory script timer",
    )
    parser.add_argument(
        "--dotted-expression-probe",
        action="store_true",
        help="evaluate dotted reference expressions and commands at login",
    )
    parser.add_argument(
        "--arg-locals-probe",
        action="store_true",
        help="exercise named ARG locals, positional object roots, and LASTNEW",
    )
    parser.add_argument(
        "--findarg-probe",
        action="store_true",
        help="exercise resource-reference event add, deduplication, and removal",
    )
    parser.add_argument(
        "--dword-hex-probe",
        action="store_true",
        help="exercise Sphere 0-prefixed hexadecimal script values",
    )
    parser.add_argument(
        "--region-weather-probe",
        action="store_true",
        help="apply region weather keys and read them back from the character sector",
    )
    parser.add_argument(
        "--timer-lifetime-item-first-probe",
        action="store_true",
        help="exercise item-first timer removal with reentrant owner removal",
    )
    parser.add_argument(
        "--timer-sibling-mutation-probe",
        action="store_true",
        help="exercise delete/reparent callbacks across three sibling lists",
    )
    parser.add_argument(
        "--timer-sibling-mutation-owner-first-probe",
        action="store_true",
        help="exercise owner-first delete/reparent callbacks across three sibling lists",
    )
    parser.add_argument(
        "--ontick-content-mutation-probe",
        action="store_true",
        help="exercise an equipped timer deleting a sibling during owner OnTick",
    )
    parser.add_argument(
        "--container-shutdown-probe",
        action="store_true",
        help="exercise an event-backed nested container reparent during shutdown",
    )
    parser.add_argument(
        "--book-pages-probe",
        action="store_true",
        help="load a BOOK with more than 127 pages and a full resource-ID ITEMDEF",
    )
    parser.add_argument(
        "--dialog-button-probe",
        action="store_true",
        help="open a dialog with numbered and ON=@anybutton button entries at login",
    )
    parser.add_argument(
        "--dialog-argo-layout-probe",
        action="store_true",
        help="open, by name, a dialog laid out with argo.<gump>(...) calls at login",
    )
    parser.add_argument(
        "--dialog-flow-layout-probe",
        action="store_true",
        help="open, by name, dialogs whose layouts use IF/WHILE/DOSWITCH/RETURN at login",
    )
    parser.add_argument(
        "--spawn-gem-probe",
        action="store_true",
        help="seed top-level spawn gems with an explicit zero timer",
    )
    parser.add_argument(
        "--spawn-gem-duplicate-serial-probe",
        action="store_true",
        help="seed duplicate spawn-gem serial sections for load handling",
    )
    parser.add_argument(
        "--events-attr-probe",
        action="store_true",
        help="exercise EVENTS, CHANGER, and ATTR across repeated save generations",
    )
    parser.add_argument(
        "--legacy-metadata-probe",
        action="store_true",
        help="exercise long unquoted TAG values and legacy named ATTR keys",
    )
    parser.add_argument(
        "--spawn-point-probe",
        action="store_true",
        help="seed a timed spawn point whose target is a quoted resource name",
    )
    parser.add_argument(
        "--escape-overflow-probe",
        action="store_true",
        help="log in an existing character through a near-limit escape expansion",
    )
    parser.add_argument(
        "--movement-stairs-probe",
        action="store_true",
        help="exercise dynamic stair height resolution",
    )
    args = parser.parse_args()

    # A mode is a complete recipe.  Reparse its declarative argument list
    # through this parser so all existing validation and defaults stay in one
    # place while each CI test can name an isolated fixture.
    if args.mode:
        args = parser.parse_args([str(args.output), *FIXTURE_MODES[args.mode]])

    world_load_modes = (
        args.truncate_world_item,
        args.unresolved_worldchar_type,
        args.noncontainer_reference,
        args.typedef_container_reference,
        args.multi_property,
        args.named_item_names,
        args.named_resource_ids,
        args.rejected_property,
        args.weird_item,
        args.child_before_parent,
        args.format_compat_probe,
        args.metadata_roundtrip_probe,
        args.character_content_probe,
    )
    if any(world_load_modes) and not args.world_load_counts:
        parser.error("world-load options require --world-load-counts")
    if sum(world_load_modes) > 1:
        parser.error("choose only one world-load fixture mode")
    if args.spawn_gem_duplicate_serial_probe:
        args.spawn_gem_probe = True
    if args.movement_stairs_probe and any(
        (
            args.world_load_counts,
            args.timer_lifetime_probe,
            args.memory_timer_probe,
            args.timer_lifetime_item_first_probe,
            args.timer_sibling_mutation_probe,
            args.timer_sibling_mutation_owner_first_probe,
            args.ontick_content_mutation_probe,
            args.container_shutdown_probe,
            args.events_attr_probe,
            args.legacy_metadata_probe,
            args.book_pages_probe,
            args.dword_hex_probe,
            args.region_weather_probe,
            args.spawn_gem_probe,
            args.spawn_point_probe,
            args.escape_overflow_probe,
        )
    ):
        parser.error("movement-stairs probe cannot be combined with another fixture mode")
    if args.escape_overflow_probe and (
        any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
        or args.book_pages_probe
        or args.dword_hex_probe
        or args.region_weather_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
        or args.spawn_point_probe
        or args.events_attr_probe
        or args.legacy_metadata_probe
    ):
        parser.error("escape-overflow probe cannot be combined with another world fixture mode")
    if (
        args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
        or args.spawn_point_probe
    ) and (
        any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
    ):
        parser.error("spawn probe cannot be combined with another world fixture mode")
    if (
        args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
    ) and any(world_load_modes):
        parser.error("timer-lifetime probe cannot be combined with a world-load mode")
    if sum(
        (
            args.timer_lifetime_probe,
            args.memory_timer_probe,
            args.timer_lifetime_item_first_probe,
            args.timer_sibling_mutation_probe,
            args.timer_sibling_mutation_owner_first_probe,
            args.ontick_content_mutation_probe,
            args.container_shutdown_probe,
        )
    ) > 1:
        parser.error("choose only one timer-lifetime probe mode")

    if args.events_attr_probe and (
        args.world_load_counts
        or any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
    ):
        parser.error("events/attr probe cannot be combined with another fixture mode")

    if args.legacy_metadata_probe and (
        args.world_load_counts
        or any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.container_shutdown_probe
        or args.events_attr_probe
        or args.book_pages_probe
        or args.dword_hex_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
    ):
        parser.error("legacy metadata probe cannot be combined with another fixture mode")

    if args.book_pages_probe and (
        args.world_load_counts
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
        or args.events_attr_probe
        or args.format_compat_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
        or args.spawn_point_probe
    ):
        parser.error("book-pages probe writes its own world and cannot be combined")
    if args.dword_hex_probe and (
        args.world_load_counts
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
        or args.book_pages_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
        or args.spawn_point_probe
    ):
        parser.error("dword-hex probe writes its own world and cannot be combined")
    if args.region_weather_probe and (
        args.world_load_counts
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.book_pages_probe
        or args.dword_hex_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
    ):
        parser.error("region-weather probe writes its own world and cannot be combined")

    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")

    write_runtime_files(
        root,
        unknown_keyword_report=args.unknown_keyword_report,
        unknown_keyword_report_format=args.unknown_keyword_report_format,
        force_garbage_collect=(
            args.timer_lifetime_probe
            or args.timer_lifetime_item_first_probe
            or args.timer_sibling_mutation_probe
            or args.timer_sibling_mutation_owner_first_probe
            or args.ontick_content_mutation_probe
            or args.container_shutdown_probe
        ),
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
        named_item_name_probe=args.named_item_names,
        named_resource_id_probe=args.named_resource_ids,
        metadata_roundtrip_probe=args.metadata_roundtrip_probe,
        timer_lifetime_probe=args.timer_lifetime_probe,
        memory_timer_probe=args.memory_timer_probe,
        dotted_expression_probe=args.dotted_expression_probe,
        format_compat_probe=args.format_compat_probe,
        arg_locals_probe=args.arg_locals_probe,
        findarg_probe=args.findarg_probe,
        dword_hex_probe=args.dword_hex_probe,
        timer_lifetime_item_first_probe=args.timer_lifetime_item_first_probe,
        timer_sibling_mutation_probe=args.timer_sibling_mutation_probe,
        timer_sibling_mutation_owner_first_probe=args.timer_sibling_mutation_owner_first_probe,
        ontick_content_mutation_probe=args.ontick_content_mutation_probe,
        container_shutdown_probe=args.container_shutdown_probe,
        book_pages_probe=args.book_pages_probe,
        dialog_button_probe=args.dialog_button_probe,
        dialog_argo_layout_probe=args.dialog_argo_layout_probe,
        dialog_flow_layout_probe=args.dialog_flow_layout_probe,
        suppress_login_item=args.roundtrip_integrity_probe,
        region_weather_probe=args.region_weather_probe,
        spawn_gem_probe=args.spawn_gem_probe,
        spawn_point_probe=args.spawn_point_probe,
        escape_overflow_probe=args.escape_overflow_probe,
        movement_stairs_probe=args.movement_stairs_probe,
        character_content_probe=args.character_content_probe,
    )
    if args.world_load_counts:
        write_world_load_counts_save(
            root,
            truncate_world_item=args.truncate_world_item,
            unresolved_worldchar_type=args.unresolved_worldchar_type,
            noncontainer_reference=args.noncontainer_reference,
            typedef_container_reference=args.typedef_container_reference,
            multi_property_reference=args.multi_property,
            named_item_names=args.named_item_names,
            named_container_reference=args.named_resource_ids,
            rejected_property=args.rejected_property,
            weird_item=args.weird_item,
            child_before_parent=args.child_before_parent,
            format_compat_probe=args.format_compat_probe,
            metadata_roundtrip_probe=args.metadata_roundtrip_probe,
            character_content_probe=args.character_content_probe,
        )
    if args.timer_lifetime_probe or args.timer_lifetime_item_first_probe:
        write_timer_lifetime_save(root)
    if args.memory_timer_probe:
        write_memory_timer_save(root)
    if args.book_pages_probe:
        write_book_pages_save(root)
    if args.dword_hex_probe:
        write_dword_hex_save(root)
    if args.region_weather_probe:
        write_region_weather_save(root)
    if args.spawn_gem_probe:
        write_spawn_gem_save(
            root,
            duplicate_serials=args.spawn_gem_duplicate_serial_probe,
        )
    if args.spawn_point_probe:
        write_spawn_point_save(root)
    if args.escape_overflow_probe:
        write_escape_overflow_save(root)
    if args.movement_stairs_probe:
        write_movement_stairs_save(root)
    if args.timer_sibling_mutation_probe or args.timer_sibling_mutation_owner_first_probe:
        write_timer_sibling_mutation_save(root)
    if args.container_shutdown_probe:
        write_container_shutdown_save(root)
    if args.ontick_content_mutation_probe:
        write_ontick_content_mutation_save(root)
    if args.events_attr_probe:
        # A zero-minute period causes the normal world-save path to run as
        # soon as the server has loaded.  The round-trip test stops after each
        # completed generation and restarts the same disposable fixture.
        ini_path = root / "sphere.ini"
        ini_path.write_text(
            ini_path.read_text(encoding="ascii").replace(
                "SAVEPERIOD=1440", "SAVEPERIOD=0"
            ),
            encoding="ascii",
        )
        write_events_attr_save(root)
    if args.legacy_metadata_probe:
        ini_path = root / "sphere.ini"
        ini_path.write_text(
            ini_path.read_text(encoding="ascii").replace(
                "SAVEPERIOD=1440", "SAVEPERIOD=0"
            ),
            encoding="ascii",
        )
        write_legacy_metadata_save(root)
    write_mul_fixture(
        root,
        extra_item_id=(
            0x0E8A
            if args.timer_sibling_mutation_probe
            or args.timer_sibling_mutation_owner_first_probe
            or args.container_shutdown_probe
            else 0
        ),
    )
    if args.timer_sibling_mutation_probe or args.timer_sibling_mutation_owner_first_probe:
        for item_id in (0x0E7D, 0x0E81, 0x0E86, 0x0E88, 0x0E89, 0x0E8A):
            write_container_tile(root / "muls" / "tiledata.mul", item_id)
    if args.container_shutdown_probe:
        for item_id in (0x0E7D, 0x0E88):
            write_container_tile(root / "muls" / "tiledata.mul", item_id)
    if args.movement_stairs_probe:
        tiledata = root / "muls" / "tiledata.mul"
        write_movement_tile(
            tiledata,
            MOVEMENT_STAIRS_ID,
            0x00000200 | 0x00000400 | 0x40000000,
            10,
        )
    print(f"wrote synthetic Sphere runtime fixture to {root}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
