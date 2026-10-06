"""Shared compatibility writer used by auto-discovered fixture modes.

The command-line module is intentionally a parser and dispatcher.  Common
synthetic file primitives live here; each mode module owns the recipe that
selects them.
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from modes.fragments.dotted_expressions import (
    DOTTED_CONDITION_ROWS,
    DOTTED_EXPRESSION_ROWS,
    DOTTED_PROBE_ACCOUNT,
    DOTTED_PROBE_CAPPED_FOR,
    DOTTED_PROBE_CAPPED_WHILE,
    DOTTED_PROBE_DISPOSABLE_ID,
    DOTTED_PROBE_FINDID_ID,
    DOTTED_PROBE_ITEM_ID,
    DOTTED_PROBE_LAYER,
    DOTTED_PROBE_MARKER,
    DOTTED_PROBE_SECTOR_LIGHT,
)
from modes.fragments.expression_chain import expression_chain_scripts

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
MEMORY_STALE_ITEM_SERIAL = 202
MEMORY_TIMER_DELAY_SECONDS = 15
MEMORY_TIMER_ITEM_UID = UID_F_ITEM | MEMORY_TIMER_ITEM_SERIAL
MEMORY_STALE_ITEM_UID = UID_F_ITEM | MEMORY_STALE_ITEM_SERIAL
MEMORY_TIMER_ITEM_ID = 0x0EA3
MEMORY_TIMER_MARKER = "SPHERE_MEMORY_TIMER_TRIGGERED"
MEMORY_TIMER_REMOVED_MARKER = "SPHERE_MEMORY_TIMER_REMOVED"
TIMER_DEFAULT_REMOVE_OWNER_SERIAL = 300
TIMER_DEFAULT_REMOVE_ITEM_SERIAL = 301
TIMER_DEFAULT_REMOVE_DELAY_SECONDS = 15
TIMER_DEFAULT_REMOVE_ITEM_UID = UID_F_ITEM | TIMER_DEFAULT_REMOVE_ITEM_SERIAL
TIMER_DEFAULT_REMOVE_ITEM_ID = 0x0EA3
TIMER_DEFAULT_REMOVE_MARKER = "SPHERE_TIMER_DEFAULT_REMOVE_TRIGGERED"
TIMER_DEFAULT_REMOVE_AFTER_MARKER = "SPHERE_TIMER_DEFAULT_REMOVE_AFTER"
TIMER_DEFAULT_HANDLER_ITEM_SERIAL = 302
TIMER_DEFAULT_HANDLER_ITEM_UID = UID_F_ITEM | TIMER_DEFAULT_HANDLER_ITEM_SERIAL
TIMER_DEFAULT_HANDLER_ITEM_ID = 0x0EA4
TIMER_DEFAULT_HANDLER_MARKER = "SPHERE_TIMER_DEFAULT_HANDLER_TRIGGERED"
CHARACTER_CONTENT_ACCOUNT = "CharacterContentProbe"
CHARACTER_CONTENT_PASSWORD = "char_content_pw"
CHARACTER_CONTENT_CHAR_SERIAL = 3
CHARACTER_CONTENT_ITEM_SERIAL = 4
CHARACTER_CONTENT_ITEM_ID = 0x0E9E
CHARACTER_CONTENT_LAYERED_ITEM_SERIAL = 5
CHARACTER_CONTENT_LAYERED_ITEM_ID = 0x0E9F
CHARACTER_CONTENT_LAYERED_ITEM_LAYER = 8
CHARACTER_CONTENT_SPECIAL_ITEM_SERIAL = 6
CHARACTER_CONTENT_SPECIAL_ITEM_ID = 0x204E
CHARACTER_CONTENT_SPECIAL_ITEM_LAYER = 22
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
ESCAPE_OVERFLOW_FORM_MARKERS = (
    "ESCAPE_OVERFLOW_SETTER",
    "ESCAPE_OVERFLOW_ARGUMENT",
    "ESCAPE_OVERFLOW_PLAIN",
    "ESCAPE_OVERFLOW_MACRO",
)
ESCAPE_OVERFLOW_NAME = "N" * 256

# Daily-log parity uses a separate empty account so the probe covers the
# character-creation record without changing the existing escape fixture.
DAILY_LOG_CREATE_ACCOUNT = "DailyCreate"
DAILY_LOG_CREATE_PASSWORD = "daily_create_pw"
DAILY_LOG_CREATE_NAME = "DailyProbe"

# The command-log probe uses the same synthetic existing-character path as the
# daily logging fixture, then dispatches twelve marker-bearing commands through
# the script-level accMsg helper.  The markers are intentionally generic so the
# fixture checks only the public command-log contract.
GM_COMMAND_LOG_MARKERS = tuple(
    f"TEST: SUCCESS - GM command marker {index:02d}" for index in range(1, 13)
)
GM_COMMAND_LOG_MARKER = "GM_COMMAND_LOG_MARKER"
GM_COMMAND_LOG_ACCOUNT = "GmCommandLogProbe"
GM_COMMAND_LOG_PASSWORD = "gm_cmd_log_pw"
GM_COMMAND_LOG_CHAR_NAME = "GmCommandLogCharacter"
GM_COMMAND_LOG_PLAYER_ACCOUNT = "GmCommandLogPlayer"
GM_COMMAND_LOG_PLAYER_PASSWORD = "gm_cmd_player_pw"

# Dedicated runaway-loop fixture.  The normal engine default remains
# generous; this mode sets a small value so the bounded return and the
# second-client check complete quickly.
RUNAWAY_LOOP_LIMIT = 32
RUNAWAY_LOOP_MARKER = "SPHERE_RUNAWAY_LOOP"

# Trigger/function recursion probe. Both paths deliberately recurse without
# a script-level exit; the engine guard must return them safely and leave a
# second client responsive.
RECURSION_DEPTH_MARKER = "SPHERE_RECURSION_DEPTH"
RECURSION_DEPTH_ACCOUNT_ONE = "RecursionDepthOne"
RECURSION_DEPTH_ACCOUNT_TWO = "RecursionDepthTwo"
RECURSION_DEPTH_PASSWORD = "recursion_pw"
RECURSION_DEPTH_LIMIT = 40

# Same-definition stacking probe.  Two movable, stackable ground items are
# dragged into the character's pack at the same explicit point.  A second
# equal-definition item is added by script without a point, exercising the
# existing pile's contained location.
STACKING_ACCOUNT = "StackingProbe"
STACKING_PASSWORD = "stacking_pw"
STACKING_ITEM_ID = 0x0E96
STACKING_NO_POINT_ITEM_ID = 0x0E97
STACKING_ITEM_SERIALS = (100, 101)
STACKING_NO_POINT_ITEM_SERIAL = 102

# A tiledata-marked container with no TDATA2 gump uses GUMP_NONE.  The
# loader must use the reserved container dimensions for a child without a
# saved point, without reporting a spurious unknown-gump error.
GUMP_FALLBACK_ITEM_ID = 0x0EA4
GUMP_FALLBACK_CHILD_ITEM_ID = 0x0EA5
GUMP_FALLBACK_CONTAINER_SERIAL = 4
GUMP_FALLBACK_CHILD_SERIAL = 5

# A script alias for TYPEDEF 0 must retain the valid IT_NORMAL index while a
# saved item is loaded.  The probe reuses the standard fixture item id so the
# reference server sees the same valid tile definition.
SCRIPT_ITEM_TYPE_SERIAL = 4
SCRIPT_ITEM_TYPE_MARKER = "SPHERE_SCRIPT_ITEM_TYPE"


# Named ARG locals and positional-object probe.  The generated login trigger
# creates one synthetic item through a script-level NEWITEMSAFE wrapper, then
# passes its UID into nested functions so ARG/ARGV resolution is exercised in
# the same context shape as a real function call.  The scratch probe mirrors
# the underscore-named locals used by legacy script helpers.
ARG_LOCALS_ACCOUNT = "ArgLocalsProbe"
ARG_LOCALS_MARKER = "SPHERE_ARG_LOCALS"

# Object-root function dispatch probe.  The two CONT calls deliberately use an
# empty argument list and three positional values so ARGVCOUNT/ARGV handling
# is exercised on the same path as legacy object-root commands.  Further rows
# cover an SRC root, an item root, the space-separated form, roots that are
# not world objects (the server, a definition), and function calls inside
# escapes.
OBJECT_ROOT_DISPATCH_ACCOUNT = "ObjectRootDispatchProbe"
OBJECT_ROOT_DISPATCH_MARKER = "SPHERE_OBJECT_ROOT"
OBJECT_ROOT_DISPATCH_ITEM_ID = 0x0EA6

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
DIALOG_BUTTON_INDEXED_NAME = "dialog_button_indexed"
# Dialog calls pass the name first, followed by positional values consumed by
# the layout through ARGV.  The probe selects a different button when the
# second value is lost before layout evaluation.
DIALOG_ARGV_ACCOUNT = "DialogArgvProbe"
DIALOG_ARGV_PASSWORD = "dialog-argv-pw"
DIALOG_ARGV_MARKER = "SPHERE_DIALOG_ARGV"
DIALOG_ARGV_NAME = "d_synthetic_dialog_argv"
DIALOG_ARGV_FIRST_VALUE = 11
DIALOG_ARGV_SECOND_VALUE = 22
DIALOG_ARGV_FORWARD_BUTTON = 77
DIALOG_ARGV_WRONG_BUTTON = 78
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
# A dialog-layout tag setter is also used by the race/class flow.  The
# comma-form ARGO.TAG(name,value) must survive layout construction so the
# button handler can dispatch the stored command.
DIALOG_ARGO_TAG_ACCOUNT = "DialogArgoTagProbe"
DIALOG_ARGO_TAG_MARKER = "SPHERE_DIALOG_ARGO_TAG"
DIALOG_ARGO_TAG_NAME = "d_synthetic_argo_tag"
DIALOG_ARGO_TAG_BUTTON = 13
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

# ISBIT probe.  0.99 takes a value and a zero-based bit position and returns
# one when that bit is set.  Include both escape forms and a high DWORD bit so
# the fixture covers the script-facing unsigned conversion.
ISBIT_ACCOUNT = "IsBitProbe"
ISBIT_PASSWORD = "isbit-pw"
ISBIT_MARKER = "SPHERE_ISBIT"

# FOOD property probe.  The existing character stat path and the item-event
# default-object path are kept in one fixture so a script-facing FOOD write
# cannot silently turn into a no-op.
FOOD_ACCOUNT = "FoodProbe"
FOOD_PASSWORD = "food-pw"
FOOD_MARKER = "SPHERE_FOOD"
FOOD_ITEM_ID = 0x0E8C
FOOD_INITIAL = 7

# Damage trigger probe.  The item callback and the source-character callback
# use distinct markers so the fixture proves both dispatch paths for one
# damage operation.
DAMAGE_TRIGGER_ACCOUNT = "DamageTriggerProbe"
DAMAGE_TRIGGER_PASSWORD = "dmgtrig-pw"
DAMAGE_TRIGGER_MARKER = "SPHERE_DAMAGE_TRIGGER"
DAMAGE_TRIGGER_ITEM_ID = 0x0E76
DAMAGE_TRIGGER_ITEM_SERIAL = 4
DAMAGE_TRIGGER_ITEM_UID = UID_F_ITEM | DAMAGE_TRIGGER_ITEM_SERIAL

# Direct EVENTS(...) method probe.  The property form (EVENTS=...) already
# has coverage in the metadata round-trip fixture; this exercises the
# statement dispatcher used by production scripts.
EVENTS_METHOD_ACCOUNT = "EventsMethodProbe"
EVENTS_METHOD_PASSWORD = "events-method-pw"
EVENTS_METHOD_MARKER = "SPHERE_EVENTS_METHOD"
EVENTS_METHOD_EVENT = "e_fixture_events_method"

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


def write_equipment_tile(path: Path, item_id: int, layer: int) -> None:
    """Mark one synthetic item as a valid visible equipment tile."""

    record_offset = (
        terrain_size()
        + ((item_id // TILE_BLOCK_QTY) * 4)
        + 4
        + (item_id * ITEM_RECORD_BYTES)
    )
    # UFLAG1_EQUIP, movable weight, and the default paperdoll layer.
    record = struct.pack(
        "<IBBIIHB20s",
        0x00000002,
        1,
        layer,
        0,
        0,
        0,
        1,
        b"synthetic equipment\0".ljust(20, b"\0"),
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


def write_stackable_tile(path: Path, item_id: int) -> None:
    """Mark a synthetic ground item as movable, nonblocking and pileable."""

    record_offset = (
        terrain_size()
        + ((item_id // TILE_BLOCK_QTY) * 4)
        + 4
        + (item_id * ITEM_RECORD_BYTES)
    )
    record = struct.pack(
        "<IBBIIHB20s",
        0x00000804,  # UFLAG1_NONBLOCKING | UFLAG2_STACKABLE
        1,
        0,
        0,
        0,
        0,
        1,
        b"synthetic stack item\0".ljust(20, b"\0"),
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


def write_stacking_save(root: Path) -> None:
    """Seed explicit-point and no-point same-definition ground piles."""

    first_serial, second_serial = STACKING_ITEM_SERIALS
    header = [
        "TITLE=Sphere synthetic item-stacking fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    write_text(root / "accounts" / "sphereaccu.scp", "\n".join(
        [
            f"[ACCOUNT {STACKING_ACCOUNT}]",
            f"PASSWORD={STACKING_PASSWORD}",
            "PLEVEL=Admin",
            "CHARUID=3",
            "LASTCHARUID=3",
            "[EOF]",
        ]
    ))
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")
    write_text(root / "save" / "sphereworld.scp", "\n".join(header + ["[EOF]"]))
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            header
            + [
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={STACKING_ACCOUNT}",
                "NAME=StackingProbeCharacter",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
                "[WORLDITEM DEFAULTITEM]",
                "SERIAL=4",
                "LAYER=21",
                "CONT=3",
                "TIMERD=-1",
                "[WORLDITEM SYNTHETIC_STACK_ITEM]",
                f"SERIAL={first_serial}",
                "P=129,128,0",
                "AMOUNT=1",
                "TIMERD=-1",
                "[WORLDITEM SYNTHETIC_STACK_ITEM]",
                f"SERIAL={second_serial}",
                "P=130,128,0",
                "AMOUNT=1",
                "TIMERD=-1",
                "[WORLDITEM SYNTHETIC_NO_POINT_STACK_ITEM]",
                f"SERIAL={STACKING_NO_POINT_ITEM_SERIAL}",
                "CONT=4",
                "P=70,70,0",
                "AMOUNT=1",
                "TIMERD=-1",
                "[EOF]",
            ]
        ),
    )


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
ARG(gata,<LASTNEW>)
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
SYSMESSAGE SPHERE_ARG_LOCALS C|gata_before|[<gata.name>|<GATA.NAME>|<GATA.SERIAL>]
ARGV(0).TYPE=T_NORMAL
SYSMESSAGE SPHERE_ARG_LOCALS C|object_after|[<argobj.type>|<ARGV(0).TYPE>]
GATA.COLOR=0123
GATA.NAME=synthetic gata
SYSMESSAGE SPHERE_ARG_LOCALS C|gata_after|[<GATA.COLOR>|<gata.name>|<GATA.SERIAL>]
GATA.SFX(248)
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


def object_root_dispatch_scripts() -> tuple[str, str]:
    """Return the object-root call probe.

    The login part reports the two CONT calls.  The functions below are also
    called from the item trigger through an SRC root, through roots that are
    not world objects, and from escapes; see object_root_dispatch_trigger().
    """

    login = [
        "NEWITEM SYNTHETIC_OBJECT_ROOT",
        "EQUIPLAST",
        f"SYSMESSAGE {OBJECT_ROOT_DISPATCH_MARKER} C|empty|[<TAG.object_root_empty>]",
        f"SYSMESSAGE {OBJECT_ROOT_DISPATCH_MARKER} C|args|[<TAG.object_root_args>]",
        f"SYSMESSAGE {OBJECT_ROOT_DISPATCH_MARKER} C_END",
    ]
    sections = f"""
[FUNCTION f_object_root_empty]
ARG(i,0)
ARG(last,)
WHILE (<ARG(i)> < ARGVCOUNT-1)
ARG(last,<ARGV(<ARG(i)>)>)
ARG(i,#+1)
ENDWHILE
TAG.object_root_empty=<ARGVCOUNT>|<ARG(i)>|<ARG(last)>
RETURN 0

[FUNCTION f_object_root_args]
ARG(i,0)
ARG(last,)
WHILE (arg(i)<ARGVCOUNT-1)
ARG(last,<?argv(<arg(i)>)?>)
ARG(i,<arg(i)>+1)
ENDWHILE
TAG.object_root_args=<ARGVCOUNT>|<ARG(i)>|<ARG(last)>
RETURN 0

[FUNCTION f_object_root_report]
TAG.object_root_report=<ARGVCOUNT>|<ARGV(0)>|<ARGV(1)>
RETURN 0

[FUNCTION f_object_root_serv]
VAR(object_root_serv,ran)
RETURN 0

[FUNCTION f_object_root_definition]
VAR(object_root_definition,ran)
RETURN 0

[FUNCTION f_object_root_inc]
RETURN <EVAL <ARGV(0)>+1>
"""
    return "\n".join(login) + "\n", sections


def object_root_dispatch_trigger() -> str:
    """Return the item trigger body of the object-root probe.

    The trigger runs with the item as the default object and the equipping
    character as both CONT and SRC.  Each row is reported as soon as its call
    returns, so a call that does not run leaves an empty value.
    """

    marker = OBJECT_ROOT_DISPATCH_MARKER
    return "\n".join(
        [
            "CONT.f_object_root_empty()",
            "CONT.f_object_root_args(1,2,3)",
            # The source character is a world object as well.
            "SRC.f_object_root_report(7,8)",
            f"SRC.SYSMESSAGE {marker} C|src_call|[<SRC.TAG.object_root_report>]",
            # So is an item: here the equipped item itself, found by its UID.
            "FINDUID(<SERIAL>).f_object_root_report(5,6)",
            f"SRC.SYSMESSAGE {marker} C|item_root|[<TAG.object_root_report>]",
            # The space-separated form keeps the argument semantics it has
            # without a root: the text is in ARGS/ARGV, the count stays zero.
            "SRC.f_object_root_report 9,10",
            f"SRC.SYSMESSAGE {marker} C|src_space|[<SRC.TAG.object_root_report>]",
            "f_object_root_report 9,10",
            f"SRC.SYSMESSAGE {marker} C|plain_space|[<TAG.object_root_report>]",
            # The server object and a definition are reference roots, but not
            # world objects: a script function is not run with them as base.
            "SERV.f_object_root_serv()",
            f"SRC.SYSMESSAGE {marker} C|serv|[<VAR(object_root_serv)>]",
            "FINDRES(ITEMDEF,SYNTHETIC_OBJECT).f_object_root_definition()",
            f"SRC.SYSMESSAGE {marker} C|definition|[<VAR(object_root_definition)>]",
            # The same two functions do run on a world object.
            "CONT.f_object_root_serv()",
            "CONT.f_object_root_definition()",
            f"SRC.SYSMESSAGE {marker} C|live_control|"
            "[<VAR(object_root_serv)>|<VAR(object_root_definition)>]",
            # Function calls inside escapes are resolved by the expression
            # evaluator, with and without a root.
            f"SRC.SYSMESSAGE {marker} C|escape|"
            "[<?f_object_root_inc(41)?>|<SRC.f_object_root_inc(7)>]",
        ]
    ) + "\n"


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

[DEFNAMES dialog_button_probe]
{DIALOG_BUTTON_INDEXED_NAME}[5] indexed-five

[DIALOG {name} BUTTON]
ON=@anybutton
TAG.dialog_button_seen=any/<ARGN>
SYSMESSAGE {marker} any|<ARGN>|<ARGCHK({DIALOG_BUTTON_SWITCH})>|<ARGTXT({DIALOG_BUTTON_TEXT_ID})>|<ARGO.NAME>
DIALOG {name}
ON={DIALOG_BUTTON_NUMBERED}
SYSMESSAGE {marker} numbered|<ARGN>|<{DIALOG_BUTTON_INDEXED_NAME}[ARGN]>
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


def dialog_argv_scripts() -> tuple[str, str]:
    """Return a dialog call with positional values visible to its layout."""

    marker = DIALOG_ARGV_MARKER
    name = DIALOG_ARGV_NAME
    login = (
        f"DIALOG({name},{DIALOG_ARGV_FIRST_VALUE},{DIALOG_ARGV_SECOND_VALUE})\n"
    )
    sections = f"""
[DIALOG {name}]
0 0
IF (<ARGV(1)> == {DIALOG_ARGV_SECOND_VALUE})
button 20 20 2151 2152 1 0 {DIALOG_ARGV_FORWARD_BUTTON}
ELSE
button 20 20 2151 2152 1 0 {DIALOG_ARGV_WRONG_BUTTON}
ENDIF

[DIALOG {name} BUTTON]
ON={DIALOG_ARGV_FORWARD_BUTTON}
SYSMESSAGE {marker} forward
ON={DIALOG_ARGV_WRONG_BUTTON}
SYSMESSAGE {marker} wrong
"""
    return login, sections


def dialog_argo_tag_scripts() -> tuple[str, str]:
    """Return a dialog whose quoted argument drives an ARGO.TAG command."""

    login = f'DIALOG({DIALOG_ARGO_TAG_NAME},"f_dialog_argo_tag_forward")\n'
    sections = f"""
[DIALOG {DIALOG_ARGO_TAG_NAME}]
0 0
ARGO.TAG(forward,<ARGV(0)>)
button 20 20 2151 2152 1 0 {DIALOG_ARGO_TAG_BUTTON}

[DIALOG {DIALOG_ARGO_TAG_NAME} BUTTON]
ON={DIALOG_ARGO_TAG_BUTTON}
<ARGO.TAG(forward)>

[FUNCTION f_dialog_argo_tag_forward]
SYSMESSAGE {DIALOG_ARGO_TAG_MARKER} forward
"""
    return login, sections


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
        "VAR dotted_cont_target,<SRC.FINDLAYER(21).SERIAL>",
        "NEWITEM SYNTHETIC_DOTTED_DISPOSABLE",
        "LASTNEW.CONT=<SRC.FINDLAYER(21)>",
        f"SYSMESSAGE {marker} C|cont_target_serial|[<VAR(dotted_cont_target)>]",
        f"SYSMESSAGE {marker} C|cont_object_serial|[<LASTNEW.CONT.SERIAL>]",
        "LASTNEW.REMOVE",
        "NEWITEM SYNTHETIC_DOTTED_PROBE",
        "EQUIPLAST",
        "NEWITEM SYNTHETIC_DOTTED_FINDID",
        "LASTNEW.CONT=<SRC.FINDLAYER(LAYER_PACK).SERIAL>",
        "LASTNEW.NAME=synthetic dotted probe",
        # A standalone object escape is a legacy empty argument. Serializing
        # <ARGO> here would feed the reference UID back into damage_final and
        # re-enter @GetHit indefinitely.
        "NEWNPC c_MAN",
        "VAR dotted_object_escape_source,<LASTNEW.SERIAL>",
        "VAR dotted_object_escape_hits,0",
        f"SYSMESSAGE {marker} C|object_escape_source|[<VAR(dotted_object_escape_source)>|<ISUIDVALID <VAR(dotted_object_escape_source)>>]",
        "TRIGGER @GetHit,1,dotted,<VAR(dotted_object_escape_source)>",
        f"SYSMESSAGE {marker} C|object_escape_hits|[<VAR(dotted_object_escape_hits)>]",
    ]
    login += dotted_expression_lines("C", "SYSMESSAGE")
    login += [
        # Keep the function-root lifetime check on a live object.  Before UID
        # quarantine the removed disposable happened to become valid again
        # when a later item reused its slot, which hid the intended check.
        "NEWITEM SYNTHETIC_DOTTED_DISPOSABLE",
        "VAR dotted_disposable,<LASTNEW.SERIAL>",
        # Commands whose left side is a reference.
        "TAG.cmd_base_set=23",
        "SRC.TAG.cmd_src_set=21",
        # 0.99 accepts a dotted property write with a space separator, not
        # only the key=value spelling.
        "SRC.TAG.cmd_space_set 22",
        "F_DOTTED_SERIAL.TAG.cmd_function_set=30",
        "FINDUID(1).TAG.cmd_finduid_set=41",
        # Global VAR dotted writes and server LOG property writes use the same
        # command splitter as object-root commands.
        "VAR.dotted_dot_set=44",
        "VAR(command_var_call,5)",
        "SERV.LOG command_probe.log",
        f"SYSMESSAGE {marker} C|cmd_space_tag|[<tag(cmd_space_set)>]",
        f"SYSMESSAGE {marker} C|cmd_var_dot|[<VAR.dotted_dot_set>]",
        f"SYSMESSAGE {marker} C|cmd_var_call|[<VAR(command_var_call)>]",
        f"SYSMESSAGE {marker} C|cmd_serv_log|[<SERV.LOG>]",
        "SRC.SYSMESSAGE " + marker + " C|cmd_src_method|[reached]",
        # The legacy command form resolves an object method before dispatching
        # the dotted operation.  These writes are read back below so a parser
        # that only treats the root as a property fails first.
        "FINDLAYER(30).TAG(bare_findlayer,11)",
        "FINDID(i_dotted_findid).TAG(bare_findid,12)",
        "SRC.FINDLAYER.30.TAG(bare_legacy_command,13)",
        "SYSMESSAGE " + marker + " C|cmd_readback|[<tag(cmd_base_set)>|<tag(cmd_src_set)>|"
        "<tag(cmd_function_set)>|<tag(cmd_finduid_set)>|<tag(cmd_item_src_set)>]",
        "SYSMESSAGE " + marker + " C|bare_method_readback|[<findlayer(30).tag(bare_findlayer)>|"
        "<findid(i_dotted_findid).tag(bare_findid)>|<findlayer(30).tag(bare_legacy_command)>]",
        "SYSMESSAGE " + marker + " C|legacy_command|[reached]",
        "SRC.NAME=DottedRenamed",
        "SYSMESSAGE " + marker + " C|cmd_src_name_set|[<name>]",
        "NAME=" + DOTTED_PROBE_ACCOUNT,
        # ARGV accepts an expression index.  The fixture passes a number and
        # an object, then selects the second value through a local counter so
        # both the scalar and referenced-object forms are covered together.
        "F_DOTTED_ARGV(42,<SRC.SERIAL>)",
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
    login += [
        "ON=@GetHit",
        f"SYSMESSAGE {marker} C|object_escape_handler|[hit]",
        "VAR dotted_object_escape_hits,<EVAL <VAR(dotted_object_escape_hits)>+1>",
        "F_DOTTED_DAMAGE_FINAL(<ARGO>)",
        "RETURN 1",
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
        # The stock item scripts use UID.DUPE in callbacks.  Keep the count
        # local to this callback so the regression proves that the method was
        # dispatched on the item exactly once.
        "VAR uid_dupe_items_before,<SERV.ITEMS>",
        "UID.DUPE 1",
        "SRC.SYSMESSAGE " + marker + " I|uid_dupe_item_delta|[<EVAL <SERV.ITEMS>-<VAR(uid_dupe_items_before)>>]",
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
        f"\n[ITEMDEF 0x{DOTTED_PROBE_FINDID_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_DOTTED_FINDID\n"
        "NAME=synthetic dotted findid item\n"
        "TYPE=T_NORMAL\n"
        "\n[DEFNAMES dotted_probe]\n"
        "dotted_probe_const 1234\n"
        "i_dotted_probe 0x0E7B\n"
        f"i_dotted_findid 0x{DOTTED_PROBE_FINDID_ID:04X}\n"
        "layer_pack 21\n"
        "str 0\n"
        "\n[FUNCTION f_dotted_serial]\n"
        "RETURN <SERIAL>\n"
        "\n[FUNCTION fixNumber]\n"
        "arg(len,<strlen(<args>)>)\n"
        "if (len<=0)\n"
        "  return 0\n"
        "endif\n"
        "arg(pattern,[-0123456789])\n"
        "while (arg(len)>1)\n"
        "  arg(pattern,\"<arg(pattern)>[-0123456789]\")\n"
        "  arg(len,<eval arg(len)-1>)\n"
        "endwhile\n"
        "if (STRMATCH(<args>,<arg(pattern)>))\n"
        "  return <eval args>\n"
        "else\n"
        "  return 0\n"
        "endif\n"
        "\n[FUNCTION fixNumberPositive]\n"
        "arg.number=<fixNumber(<args>)>\n"
        "if (<arg.number> < 0)\n"
        "  return 0\n"
        "else\n"
        "  return <arg.number>\n"
        "endif\n"
        "\n[FUNCTION f_dotted_bare_probe]\n"
        "VAR dotted_bare_calls,<EVAL <VAR(dotted_bare_calls)>+1>\n"
        "RETURN 1\n"
        "\n[FUNCTION f_dotted_arg]\n"
        "RETURN <ARGS>\n"
        "\n[FUNCTION f_dotted_argv]\n"
        "ARG(i,1)\n"
        "SRC.SYSMESSAGE SPHERE_DOTTED_EXPR C|argv_index|[<ARGV(i)>|<ARGV(i).TYPE>|<ARGV(i).NAME>|<ARGV(i).SERIAL>]\n"
        "RETURN 1\n"
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
        "\n[FUNCTION f_dotted_damage_final]\n"
        f"SYSMESSAGE {marker} C|object_escape_arg|[<ARGV(0)>]\n"
        "IF (<ARGV(0)>)\n"
        "DAMAGE 1,2,<ARGV(0)>\n"
        "ENDIF\n"
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
    script_item_type_probe: bool = False,
    world_save_probe: bool = False,
    unresolved_worldchar_type: bool = False,
    typedef_container_probe: bool = False,
    multi_property_probe: bool = False,
    named_item_name_probe: bool = False,
    named_resource_id_probe: bool = False,
    timer_lifetime_probe: bool = False,
    memory_timer_probe: bool = False,
    timer_default_remove_probe: bool = False,
    dotted_expression_probe: bool = False,
    format_compat_probe: bool = False,
    arg_locals_probe: bool = False,
    object_root_dispatch_probe: bool = False,
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
    dialog_argv_probe: bool = False,
    dialog_argo_tag_probe: bool = False,
    dword_hex_probe: bool = False,
    isbit_probe: bool = False,
    food_probe: bool = False,
    damage_trigger_probe: bool = False,
    events_method_probe: bool = False,
    region_weather_probe: bool = False,
    suppress_login_item: bool = False,
    spawn_gem_probe: bool = False,
    spawn_point_probe: bool = False,
    metadata_roundtrip_probe: bool = False,
    escape_overflow_probe: bool = False,
    gm_command_log_probe: bool = False,
    runaway_loop_probe: bool = False,
    recursion_depth_probe: bool = False,
    movement_stairs_probe: bool = False,
    character_content_probe: bool = False,
    stacking_probe: bool = False,
    gump_fallback_probe: bool = False,
    expression_chain_probe: bool = False,
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
    object_root_dispatch_login, object_root_dispatch_sections = (
        object_root_dispatch_scripts() if object_root_dispatch_probe else ("", "")
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
    expression_chain_login, expression_chain_sections = (
        expression_chain_scripts() if expression_chain_probe else ("", "")
    )
    dialog_argv_login, dialog_argv_sections = (
        dialog_argv_scripts() if dialog_argv_probe else ("", "")
    )
    dialog_argo_tag_login, dialog_argo_tag_sections = (
        dialog_argo_tag_scripts() if dialog_argo_tag_probe else ("", "")
    )
    dword_hex_login, dword_hex_sections = (
        dword_hex_scripts() if dword_hex_probe else ("", "")
    )
    isbit_login = (
        f"SYSMESSAGE {ISBIT_MARKER} C|bits|<ISBIT 5,0>|<ISBIT 5,1>|"
        f"<ISBIT(5,2)>|<ISBIT 080000000,31>|<ISBIT 5,32>\n"
        f"SYSMESSAGE {ISBIT_MARKER}_END\n"
        if isbit_probe
        else ""
    )
    food_probe_login = (
        f"FOOD={FOOD_INITIAL}\n"
        f"SYSMESSAGE {FOOD_MARKER} C|<FOOD>|<SRC.FOOD>\n"
        "NEWITEM SYNTHETIC_FOOD_ITEM\n"
        "EQUIPLAST\n"
        f"SYSMESSAGE {FOOD_MARKER}_END\n"
        if food_probe
        else ""
    )
    damage_trigger_login = (
        f"FINDUID({DAMAGE_TRIGGER_ITEM_UID}).DAMAGE 1,2,<SRC.SERIAL>\n"
        f"SYSMESSAGE {DAMAGE_TRIGGER_MARKER}_END\n"
        if damage_trigger_probe
        else ""
    )
    events_method_login = (
        f"EVENTS(+{EVENTS_METHOD_EVENT})\n"
        f"SYSMESSAGE {EVENTS_METHOD_MARKER}_ADD <EVENTS>\n"
        f"EVENTS(-{EVENTS_METHOD_EVENT})\n"
        f"SYSMESSAGE {EVENTS_METHOD_MARKER}_REMOVE <EVENTS>\n"
        f"EVENTS=-{EVENTS_METHOD_EVENT}\n"
        f"SYSMESSAGE {EVENTS_METHOD_MARKER}_PROPERTY_REMOVE <EVENTS>\n"
        f"SYSMESSAGE {EVENTS_METHOD_MARKER}_END\n"
        if events_method_probe
        else ""
    )
    events_method_sections = (
        f"\n[EVENTS {EVENTS_METHOD_EVENT}]\nON=@LogIn\nRETURN 0\n"
        if events_method_probe
        else ""
    )
    damage_trigger_itemdef = (
        "ON=@Damage\n"
        f"SRC.SYSMESSAGE {DAMAGE_TRIGGER_MARKER} ITEM\n"
        "RETURN 0\n"
        if damage_trigger_probe
        else ""
    )
    object_root_dispatch_itemdef = (
        f"\n[ITEMDEF 0x{OBJECT_ROOT_DISPATCH_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_OBJECT_ROOT\n"
        "NAME=synthetic object-root target\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Equip\n"
        + object_root_dispatch_trigger()
        if object_root_dispatch_probe
        else ""
    )
    damage_trigger_event = (
        "ON=@ItemDamage\n"
        f"SYSMESSAGE {DAMAGE_TRIGGER_MARKER} CHAR\n"
        "RETURN 0\n"
        if damage_trigger_probe
        else ""
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
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_LAYERED_LAYER "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT_LAYERED).LAYER>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_SPECIAL_UID "
        "<SRC.FINDID(i_deathshroud).SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_SPECIAL_PARENT "
        "<SRC.FINDID(i_deathshroud).CONT.SERIAL>\n"
        f"SYSMESSAGE {CHARACTER_CONTENT_MARKER}_SPECIAL_LAYER "
        "<SRC.FINDID(i_deathshroud).LAYER>\n"
        "SYSMESSAGE SPHERE_CHARACTER_CONTENT_UPDATE "
        "<SRC.FINDID(SYNTHETIC_CHARACTER_CONTENT_LAYERED).UPDATE>\n"
        "SERV.SAVE 1\n"
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
    script_item_type_probe_script = (
        f"SYSMESSAGE {SCRIPT_ITEM_TYPE_MARKER} <FINDUID({SCRIPT_ITEM_TYPE_SERIAL}).TYPE>\n"
        if script_item_type_probe
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
    escape_overflow_sections = ""
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
            "F_ESCAPE_OVERFLOW_SETTER\n"
            f"SYSMESSAGE {ESCAPE_OVERFLOW_FORM_MARKERS[0]}_RETURNED\n"
            "F_ESCAPE_OVERFLOW_ARGUMENT\n"
            f"SYSMESSAGE {ESCAPE_OVERFLOW_FORM_MARKERS[1]}_RETURNED\n"
            "F_ESCAPE_OVERFLOW_PLAIN\n"
            f"SYSMESSAGE {ESCAPE_OVERFLOW_FORM_MARKERS[2]}_RETURNED\n"
            "F_ESCAPE_OVERFLOW_MACRO\n"
            f"SYSMESSAGE {ESCAPE_OVERFLOW_FORM_MARKERS[3]}_RETURNED\n"
            + f"SYSMESSAGE {ESCAPE_OVERFLOW_MARKER}\n"
        )
        escape_overflow_sections = (
            "\n[FUNCTION f_escape_overflow_setter]\n"
            + escape_line
            + "\nRETURN 1\n"
            + "\n[FUNCTION f_escape_overflow_argument]\n"
            + "SYSMESSAGE " + ("P" * 3000) + "<NAME>" + ("S" * 1050) + "\n"
            + "RETURN 1\n"
            + "\n[FUNCTION f_escape_overflow_plain]\n"
            + "SAY " + ("P" * 3000) + "<NAME>" + ("S" * 1050) + "\n"
            + "RETURN 1\n"
            + "\n[FUNCTION f_escape_overflow_macro]\n"
            + "SYSMESSAGE " + ("P" * 3000) + "<?NAME?>" + ("S" * 1050) + "\n"
            + "RETURN 1\n"
        )
    gm_command_log_login = ""
    gm_command_log_sections = ""
    if gm_command_log_probe:
        gm_command_log_login = "".join(
            f"ACCMSG({marker})\n" for marker in GM_COMMAND_LOG_MARKERS
        ) + f"SYSMESSAGE {GM_COMMAND_LOG_MARKER}_DONE\n"
        gm_command_log_sections = (
            "\n[FUNCTION s]\n"
            "RETURN 0\n"

            "\n[FUNCTION charExists]\n"
            "IF (safe finduid(<ARGV(0)>).isChar)\n"
            "  RETURN 1\n"
            "ENDIF\n"
            "RETURN 0\n"

            "\n[FUNCTION accMsg]\n"
            "IF (charExists(<UID>))\n"
            "  IF (isPlayer)\n"
            "    TRY S(NAME=<ARGS>)\n"
            "  ENDIF\n"
            "ELSEIF (safe SRC.isPlayer)\n"
            "  SRC.TRY S(<ARGS>)\n"
            "  SRC.TRY INFO\n"
            "ENDIF\n"
            "RETURN 0\n"
        )
    runaway_loop_login = (
        f"SYSMESSAGE {RUNAWAY_LOOP_MARKER}_CONFIG <SERV.SCRIPTLOOPLIMIT>\n"
        "F_RUNAWAY_LOOP\n"
        if runaway_loop_probe
        else ""
    )
    runaway_loop_sections = (
        "\n[FUNCTION f_runaway_loop]\n"
        "WHILE (1)\n"
        "ENDWHILE\n"
        f"SYSMESSAGE {RUNAWAY_LOOP_MARKER}_RETURNED\n"
        "RETURN 1\n"
        if runaway_loop_probe
        else ""
    )
    recursion_depth_login = (
        f"SYSMESSAGE {RECURSION_DEPTH_MARKER}_FUNCTION_BEGIN\n"
        "F_RECURSION_DEPTH_PROBE\n"
        f"SYSMESSAGE {RECURSION_DEPTH_MARKER}_FUNCTION_RETURNED\n"
        "TRIGGER @RecursionDepthProbe\n"
        f"SYSMESSAGE {RECURSION_DEPTH_MARKER}_TRIGGER_RETURNED\n"
        if recursion_depth_probe
        else ""
    )
    recursion_depth_trigger = (
        "ON=@RecursionDepthProbe\n"
        "TRIGGER @RecursionDepthProbe\n"
        "RETURN 0\n"
        if recursion_depth_probe
        else ""
    )
    recursion_depth_sections = (
        "\n[FUNCTION f_recursion_depth_probe]\n"
        "F_RECURSION_DEPTH_PROBE\n"
        "RETURN 1\n"
        if recursion_depth_probe
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
    timer_default_remove_itemdef = (
        "\n[TYPEDEF 74]\n"
        "DEFNAME=T_EQ_MEMORY_OBJ\n"
        "\n[ITEMDEF 0x2007]\n"
        "DEFNAME=i_memory\n"
        "TYPE=T_EQ_MEMORY_OBJ\n"
        "LAYER=30\n"
        f"\n[ITEMDEF 0x{TIMER_DEFAULT_REMOVE_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_TIMER_DEFAULT_REMOVE\n"
        "NAME=synthetic timer default remove\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Timer\n"
        f"SERV.B {TIMER_DEFAULT_REMOVE_MARKER} <ISUIDVALID {TIMER_DEFAULT_REMOVE_ITEM_UID}>\n"
        "REMOVE\n"
        f"SERV.B {TIMER_DEFAULT_REMOVE_AFTER_MARKER} <ISUIDVALID {TIMER_DEFAULT_REMOVE_ITEM_UID}>\n"
        f"\n[ITEMDEF 0x{TIMER_DEFAULT_HANDLER_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_TIMER_DEFAULT_HANDLER\n"
        "NAME=synthetic timer default handler\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=31\n"
        "ON=@Timer\n"
        f"SERV.B {TIMER_DEFAULT_HANDLER_MARKER} <ISUIDVALID {TIMER_DEFAULT_HANDLER_ITEM_UID}>\n"
        if timer_default_remove_probe
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
    typedef_script_item_alias = (
        "DEFNAME=SYNTHETIC_SCRIPT_TYPE_ZERO\n"
        if script_item_type_probe
        else ""
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
    script_item_type_itemdef = (
        "DEFNAME=SYNTHETIC_SCRIPT_ITEM_TYPE\n"
        if script_item_type_probe
        else ""
    )
    default_item_type_line = (
        "TYPE = 00" if script_item_type_probe else "TYPE=CONTAINER"
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
    # Keep a timer item without an @Timer handler so the generic diagnostic
    # remains covered for genuinely unhandled timers.
    named_item_name_sections = (
        f"\n[ITEMDEF 0x{NAMED_TIMER_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_NAMED_TIMER\n"
        "NAME=synthetic timer item\n"
        "TYPE=T_NORMAL\n"
        if named_item_name_probe
        else ""
    )
    stacking_itemdef = (
        f"\n[ITEMDEF 0x{STACKING_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_STACK_ITEM\n"
        "NAME=synthetic stack item\n"
        "TYPE=T_NORMAL\n"
        "CAN=0x100\n"
        f"\n[ITEMDEF 0x{STACKING_NO_POINT_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_NO_POINT_STACK_ITEM\n"
        "NAME=synthetic no-point stack item\n"
        "TYPE=T_NORMAL\n"
        "CAN=0x100\n"
        if stacking_probe
        else ""
    )
    gump_fallback_itemdef = (
        f"\n[ITEMDEF 0x{GUMP_FALLBACK_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_GUMP_NONE_CONTAINER\n"
        "NAME=synthetic container without a gump\n"
        "TYPE=CONTAINER\n"
        f"\n[ITEMDEF 0x{GUMP_FALLBACK_CHILD_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_GUMP_NONE_CONTENT\n"
        "NAME=synthetic child of a container without a gump\n"
        "TYPE=T_NORMAL\n"
        if gump_fallback_probe
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
    character_content_char_can = "CAN=0x114\n" if character_content_probe else ""
    if unresolved_worldchar_type:
        default_char_definition = "[DEFNAMES HARDCODED]\nDEFAULTCHAR c_MAN\n\n"
        default_char_defname2 = ""
    character_content_itemdef = (
        "\n[TYPEDEF 181]\nDEFNAME=T_JEWELRY\n"
        f"\n[ITEMDEF 0x{CHARACTER_CONTENT_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_CHARACTER_CONTENT\n"
        "NAME=synthetic character content\n"
        "TYPE=T_NORMAL\n"
        f"\n[ITEMDEF 0x{CHARACTER_CONTENT_LAYERED_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_CHARACTER_CONTENT_LAYERED\n"
        "NAME=synthetic character content with default equip layer\n"
        f"LAYER={CHARACTER_CONTENT_LAYERED_ITEM_LAYER}\n"
        "TYPE=T_JEWELRY\n"
        f"\n[ITEMDEF 0x{CHARACTER_CONTENT_SPECIAL_ITEM_ID:04X}]\n"
        "DEFNAME=i_deathshroud\n"
        "NAME=synthetic character content deathshroud\n"
        "TYPE=T_NORMAL\n"
        if character_content_probe
        else ""
    )
    food_itemdef = (
        f"\n[ITEMDEF 0x{FOOD_ITEM_ID:04X}]\n"
        "DEFNAME=SYNTHETIC_FOOD_ITEM\n"
        "NAME=synthetic food item\n"
        "TYPE=T_EQ_SCRIPT\n"
        "LAYER=30\n"
        "ON=@Equip\n"
        "FOOD=0\n"
        f"SRC.SYSMESSAGE {FOOD_MARKER} ITEM|<FOOD>|<SRC.FOOD>\n"
        if food_probe
        else ""
    )
    write_text(
        root / "scripts" / "spheretables.scp",
        """; Synthetic definitions generated by tools/fixtures/make_fixture.py.
; This file contains no client or shard data.

""" + typedef_container_table + """
[TYPEDEF 0]
""" + typedef_normal_alias + typedef_script_item_alias + """
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
""" + script_item_type_itemdef + """NAME=synthetic container
""" + default_item_type_line + """
TDATA2=1

[ITEMDEF 0x0E76]
DEFNAME=SYNTHETIC_OBJECT
NAME=synthetic object
TYPE=T_NORMAL
""" + object_root_dispatch_itemdef + damage_trigger_itemdef + """

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

""" + timer_lifetime_probe_itemdefs + memory_timer_itemdef + timer_default_remove_itemdef + character_content_itemdef + food_itemdef + gump_fallback_itemdef + """
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
""" + character_content_char_can + """
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
""" + events_method_login + """
""" + world_save_logout_event_login + escape_overflow_login + gm_command_log_login + recursion_depth_login + world_save_login_probe_script + container_shutdown_login + findarg_login + timer_lifetime_baseline + timer_sibling_mutation_before_markers + """
""" + timer_lifetime_observer_login + timer_sibling_mutation_observer_login + """
""" + runaway_loop_login + """
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
""" + character_content_login + """
""" + isbit_login + """
""" + food_probe_login + """
""" + damage_trigger_login + """
""" + ("NEWITEM SYNTHETIC_NO_POINT_STACK_ITEM\nLASTNEW.CONT=4\n" if stacking_probe else "") + """
""" + ("" if timer_lifetime_probe or memory_timer_probe or timer_default_remove_probe or suppress_login_item or character_content_probe else "NEWITEM SYNTHETIC_HAIR\n") + """
""" + world_load_counts_probe_script + script_item_type_probe_script + unknown_keyword_probe_script + unknown_keyword_overflow_script + dotted_expression_login + arg_locals_login + object_root_dispatch_login + expression_chain_login + dword_hex_login + region_weather_login + dialog_button_login + dialog_argv_login + dialog_argo_tag_login + typedef_container_itemdef + multi_property_typedef + map_property_typedef + multi_property_itemdef + map_property_itemdef + damage_trigger_event + """
ON=@EnvironChange
""" + environ_change_body + """ON=@Logout
""" + ("" if suppress_login_item else world_save_probe_script) + """
RETURN 0
""" + recursion_depth_trigger + """
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
""" + dotted_expression_sections + arg_locals_sections + object_root_dispatch_sections + expression_chain_sections + dword_hex_sections + dialog_button_sections + dialog_argv_sections + dialog_argo_tag_sections + runaway_loop_sections + recursion_depth_sections + escape_overflow_sections + events_method_sections + """
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
        + book_pages_probe_sections
        + stacking_itemdef + gm_command_log_sections,
    )


def write_runtime_files(
    root: Path,
    *,
    unknown_keyword_report: bool = False,
    unknown_keyword_report_format: str = "json",
    force_garbage_collect: bool = False,
    runaway_loop_probe: bool = False,
    timer_removal_provenance: bool = False,
    debug_level: int = 0,
    gm_command_log_probe: bool = False,
    daily_logging_probe: bool = False,
) -> None:
    timer_provenance_setting = (
        "TIMERREMOVALPROVENANCE=1\n" if timer_removal_provenance else ""
    )
    unknown_keyword_report_setting = (
        f"UNKNOWNKEYWORDREPORT=logs/unknown-keywords.{unknown_keyword_report_format}\n"
        if unknown_keyword_report
        else ""
    )
    daily_logging_setting = (
        "VERBOSE=1\nLOGMASK=0x1ffff\nHEARALL=1\n"
        if daily_logging_probe
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
DEBUGLEVEL=""" + str(debug_level) + """
""" + daily_logging_setting + ("LOGMASK=0x1ffff\nVERBOSE=1\n" if gm_command_log_probe else "") + """
""" + timer_provenance_setting + """
CLIENTMAX=64
CLIENTSPERIP=64
MAXCHARS=5
SAVEPERIOD=1440
SAVEBACKGROUND=0
CLIENTLINGER=60
SECURE=1
""" + ("FORCEGARBAGECOLLECT=1\n" if force_garbage_collect else "") + (
        f"SCRIPTLOOPLIMIT={RUNAWAY_LOOP_LIMIT}\n" if runaway_loop_probe else ""
    ) + unknown_keyword_report_setting + """

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
                f"[{DAILY_LOG_CREATE_ACCOUNT}]",
                f"PASSWORD={DAILY_LOG_CREATE_PASSWORD}",
                "[EOF]",
            )
        ),
    )
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")
    # Both files of a save carry the same SAVECOUNT header; the server rejects
    # a pair in which only the character file has one.
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            (
                "TITLE=Sphere synthetic login escape fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            )
        ),
    )
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


def write_gm_command_log_save(root: Path) -> None:
    """Write one existing admin character for the command-log probe."""

    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            (
                f"[{GM_COMMAND_LOG_ACCOUNT}]",
                f"PASSWORD={GM_COMMAND_LOG_PASSWORD}",
                "PLEVEL=Admin",
                "CHARUID=1",
                "LASTCHARUID=1",
                f"[{GM_COMMAND_LOG_PLAYER_ACCOUNT}]",
                f"PASSWORD={GM_COMMAND_LOG_PLAYER_PASSWORD}",
                "PLEVEL=Player",
                "[EOF]",
            )
        ),
    )
    write_text(root / "accounts" / "sphereacct.scp", "[EOF]")
    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            (
                "TITLE=Sphere synthetic command-log fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[EOF]",
            )
        ),
    )
    write_text(
        root / "save" / "spherechars.scp",
        "\n".join(
            (
                "TITLE=Sphere synthetic command-log fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=1",
                f"ACCOUNT={GM_COMMAND_LOG_ACCOUNT}",
                f"NAME={GM_COMMAND_LOG_CHAR_NAME}",
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
    gump_fallback_probe: bool,
    script_item_type_reference: bool,
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
    elif script_item_type_reference:
        world_sections.extend(
            [
                "[WORLDITEM DEFAULTITEM]",
                f"SERIAL={SCRIPT_ITEM_TYPE_SERIAL}",
                "P=128,128,0",
                "TYPE=00",
            ]
        )
    elif gump_fallback_probe:
        # The child has no saved point, so loading must choose the reserved
        # container dimensions even though this container definition has no
        # TDATA2 gump (GUMP_NONE).
        world_sections.extend(
            [
                "[WORLDITEM SYNTHETIC_GUMP_NONE_CONTAINER]",
                f"SERIAL={GUMP_FALLBACK_CONTAINER_SERIAL}",
                "P=128,128,0",
                "[WORLDITEM SYNTHETIC_GUMP_NONE_CONTENT]",
                f"SERIAL={GUMP_FALLBACK_CHILD_SERIAL}",
                f"CONT={GUMP_FALLBACK_CONTAINER_SERIAL}",
            ]
        )
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
            "[WORLDITEM i_deathshroud]",
            f"SERIAL={CHARACTER_CONTENT_SPECIAL_ITEM_SERIAL}",
            f"CONT={CHARACTER_CONTENT_CHAR_SERIAL}",
            "[EOF]",
        ]
    elif (
        rejected_property
        or child_before_parent
        or format_compat_probe
        or metadata_roundtrip_probe
        or gump_fallback_probe
    ):
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


def write_events_method_save(root: Path) -> None:
    """Seed an existing account/character for the EVENTS method probe."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic EVENTS method fixture",
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
                f"[ACCOUNT {EVENTS_METHOD_ACCOUNT}]",
                f"PASSWORD={EVENTS_METHOD_PASSWORD}",
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
                "TITLE=Sphere synthetic EVENTS method fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={EVENTS_METHOD_ACCOUNT}",
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


def write_isbit_save(root: Path) -> None:
    """Seed an existing account/character for the ISBIT function probe."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic ISBIT fixture",
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
                f"[ACCOUNT {ISBIT_ACCOUNT}]",
                f"PASSWORD={ISBIT_PASSWORD}",
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
                "TITLE=Sphere synthetic ISBIT fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={ISBIT_ACCOUNT}",
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


def write_food_save(root: Path) -> None:
    """Seed an existing account/character for the FOOD property probe."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic FOOD fixture",
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
                f"[ACCOUNT {FOOD_ACCOUNT}]",
                f"PASSWORD={FOOD_PASSWORD}",
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
                "TITLE=Sphere synthetic FOOD fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={FOOD_ACCOUNT}",
                "EVENTS=e_AllPlayers",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                f"FOOD={FOOD_INITIAL}",
                "P=128,128,0",
                "[EOF]",
            ]
        ),
    )


def write_damage_trigger_save(root: Path) -> None:
    """Seed an existing character for the item/character damage callbacks."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic damage trigger fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDITEM SYNTHETIC_OBJECT]",
                f"SERIAL={DAMAGE_TRIGGER_ITEM_SERIAL}",
                "P=128,128,0",
                "TIMERD=-1",
                "[EOF]",
            ]
        ),
    )
    write_text(
        root / "accounts" / "sphereaccu.scp",
        "\n".join(
            [
                f"[ACCOUNT {DAMAGE_TRIGGER_ACCOUNT}]",
                f"PASSWORD={DAMAGE_TRIGGER_PASSWORD}",
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
                "TITLE=Sphere synthetic damage trigger fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                "SERIAL=3",
                f"ACCOUNT={DAMAGE_TRIGGER_ACCOUNT}",
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
                "[WORLDITEM i_memory]",
                f"SERIAL={MEMORY_STALE_ITEM_SERIAL}",
                f"CONT={MEMORY_TIMER_OWNER_SERIAL}",
                "COLOR=4",
                "LAYER=30",
                "LINK=0x0DEAD00",
                "[EOF]",
            ]
        ),
    )


def write_timer_default_remove_save(root: Path) -> None:
    """Seed an item whose timer callback removes itself without RETURN."""

    write_text(
        root / "save" / "sphereworld.scp",
        "\n".join(
            [
                "TITLE=Sphere synthetic default timer removal fixture",
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
                "TITLE=Sphere synthetic default timer removal fixture",
                "VERSION=0.99",
                "SAVECOUNT=0",
                "[WORLDCHAR c_MAN]",
                f"SERIAL={TIMER_DEFAULT_REMOVE_OWNER_SERIAL}",
                "NPC=2",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=130,128,0",
                "[WORLDITEM SYNTHETIC_TIMER_DEFAULT_REMOVE]",
                f"SERIAL={TIMER_DEFAULT_REMOVE_ITEM_SERIAL}",
                f"CONT={TIMER_DEFAULT_REMOVE_OWNER_SERIAL}",
                "LAYER=30",
                f"TIMER={TIMER_DEFAULT_REMOVE_DELAY_SECONDS}",
                "[WORLDITEM SYNTHETIC_TIMER_DEFAULT_HANDLER]",
                f"SERIAL={TIMER_DEFAULT_HANDLER_ITEM_SERIAL}",
                f"CONT={TIMER_DEFAULT_REMOVE_OWNER_SERIAL}",
                "LAYER=31",
                f"TIMER={TIMER_DEFAULT_REMOVE_DELAY_SECONDS}",
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
        "TITLE=Sphere synthetic sibling mutation fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
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


def generate_fixture(
    args: argparse.Namespace, parser: argparse.ArgumentParser
) -> int:

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
        args.gump_fallback_probe,
        args.script_item_type_reference,
    )
    if any(world_load_modes) and not args.world_load_counts:
        parser.error("world-load options require --world-load-counts")
    if sum(world_load_modes) > 1:
        parser.error("choose only one world-load fixture mode")
    if args.spawn_gem_duplicate_serial_probe:
        args.spawn_gem_probe = True
    if args.gm_command_log_probe and any(
        (
            args.world_load_counts,
            args.timer_lifetime_probe,
            args.memory_timer_probe,
            args.timer_default_remove_probe,
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
            args.daily_logging_probe,
        )
    ):
        parser.error("GM command-log probe cannot be combined with another fixture mode")
    if args.events_method_probe:
        conflicts = [
            name
            for name, value in vars(args).items()
            if name.endswith("_probe")
            and name != "events_method_probe"
            and value
        ]
        if args.world_load_counts:
            conflicts.append("world_load_counts")
        if conflicts:
            parser.error(
                "events-method probe cannot be combined with another fixture mode: "
                + ", ".join(conflicts)
            )
    if args.movement_stairs_probe and any(
        (
            args.world_load_counts,
            args.timer_lifetime_probe,
            args.memory_timer_probe,
            args.timer_default_remove_probe,
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
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
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
            args.timer_default_remove_probe,
            args.timer_lifetime_item_first_probe,
            args.timer_sibling_mutation_probe,
            args.timer_sibling_mutation_owner_first_probe,
            args.ontick_content_mutation_probe,
            args.container_shutdown_probe,
        )
    ) > 1:
        parser.error("choose only one timer-lifetime probe mode")

    if args.recursion_depth_probe and any(
        (
            args.world_load_counts,
            any(world_load_modes),
            args.timer_lifetime_probe,
            args.memory_timer_probe,
            args.timer_default_remove_probe,
            args.timer_lifetime_item_first_probe,
            args.timer_sibling_mutation_probe,
            args.timer_sibling_mutation_owner_first_probe,
            args.ontick_content_mutation_probe,
            args.container_shutdown_probe,
            args.book_pages_probe,
            args.dialog_button_probe,
            args.dialog_argo_layout_probe,
            args.dialog_flow_layout_probe,
            args.dialog_argv_probe,
            args.dialog_argo_tag_probe,
            args.dword_hex_probe,
            args.region_weather_probe,
            args.spawn_gem_probe,
            args.spawn_gem_duplicate_serial_probe,
            args.spawn_point_probe,
            args.events_attr_probe,
            args.legacy_metadata_probe,
            args.escape_overflow_probe,
            args.movement_stairs_probe,
            args.movement_stacking_probe,
        )
    ):
        parser.error("recursion-depth probe cannot be combined with another fixture mode")

    if args.events_attr_probe and (
        args.world_load_counts
        or any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
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
        or args.timer_default_remove_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.book_pages_probe
        or args.dword_hex_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
    ):
        parser.error("region-weather probe writes its own world and cannot be combined")
    if args.food_probe and (
        any(world_load_modes)
        or args.timer_lifetime_probe
        or args.memory_timer_probe
        or args.timer_default_remove_probe
        or args.timer_lifetime_item_first_probe
        or args.timer_sibling_mutation_probe
        or args.timer_sibling_mutation_owner_first_probe
        or args.ontick_content_mutation_probe
        or args.container_shutdown_probe
        or args.book_pages_probe
        or args.dword_hex_probe
        or args.isbit_probe
        or args.region_weather_probe
        or args.spawn_gem_probe
        or args.spawn_gem_duplicate_serial_probe
        or args.spawn_point_probe
        or args.events_attr_probe
        or args.legacy_metadata_probe
    ):
        parser.error("food probe writes its own world and cannot be combined")

    root = args.output.resolve()
    root.mkdir(parents=True, exist_ok=True)
    if any(root.iterdir()):
        parser.error(f"output directory is not empty: {root}")

    write_runtime_files(
        root,
        unknown_keyword_report=args.unknown_keyword_report,
        unknown_keyword_report_format=args.unknown_keyword_report_format,
        runaway_loop_probe=(
            args.runaway_loop_probe
            or args.expression_chain_probe
            or args.daily_logging_probe
        ),
        timer_removal_provenance=(
            args.memory_timer_probe or args.timer_default_remove_probe
        ),
        debug_level=2 if args.daily_logging_probe else 0,
        gm_command_log_probe=args.gm_command_log_probe,
        daily_logging_probe=args.daily_logging_probe,
        force_garbage_collect=(
            args.timer_lifetime_probe
            or args.timer_lifetime_item_first_probe
            or args.timer_sibling_mutation_probe
            or args.timer_sibling_mutation_owner_first_probe
            or args.ontick_content_mutation_probe
            or args.container_shutdown_probe
            or args.character_content_probe
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
        gm_command_log_probe=args.gm_command_log_probe,
        timer_lifetime_probe=args.timer_lifetime_probe,
        memory_timer_probe=args.memory_timer_probe,
        timer_default_remove_probe=args.timer_default_remove_probe,
        dotted_expression_probe=args.dotted_expression_probe,
        format_compat_probe=args.format_compat_probe,
        arg_locals_probe=args.arg_locals_probe,
        object_root_dispatch_probe=args.object_root_dispatch_probe,
        findarg_probe=args.findarg_probe,
        dword_hex_probe=args.dword_hex_probe,
        isbit_probe=args.isbit_probe,
        food_probe=args.food_probe,
        damage_trigger_probe=args.damage_trigger_probe,
        events_method_probe=args.events_method_probe,
        timer_lifetime_item_first_probe=args.timer_lifetime_item_first_probe,
        timer_sibling_mutation_probe=args.timer_sibling_mutation_probe,
        timer_sibling_mutation_owner_first_probe=args.timer_sibling_mutation_owner_first_probe,
        ontick_content_mutation_probe=args.ontick_content_mutation_probe,
        container_shutdown_probe=args.container_shutdown_probe,
        book_pages_probe=args.book_pages_probe,
        dialog_button_probe=args.dialog_button_probe,
        dialog_argo_layout_probe=args.dialog_argo_layout_probe,
        dialog_flow_layout_probe=args.dialog_flow_layout_probe,
        dialog_argv_probe=args.dialog_argv_probe,
        dialog_argo_tag_probe=args.dialog_argo_tag_probe,
        suppress_login_item=(
            args.roundtrip_integrity_probe
            or args.damage_trigger_probe
            or args.events_method_probe
        ),
        region_weather_probe=args.region_weather_probe,
        spawn_gem_probe=args.spawn_gem_probe,
        spawn_point_probe=args.spawn_point_probe,
        escape_overflow_probe=(args.escape_overflow_probe or args.daily_logging_probe),
        runaway_loop_probe=(args.runaway_loop_probe or args.daily_logging_probe),
        recursion_depth_probe=args.recursion_depth_probe,
        movement_stairs_probe=args.movement_stairs_probe,
        character_content_probe=args.character_content_probe,
        stacking_probe=args.movement_stacking_probe,
        gump_fallback_probe=args.gump_fallback_probe,
        script_item_type_probe=args.script_item_type_reference,
        expression_chain_probe=args.expression_chain_probe,
    )
    if args.movement_stacking_probe:
        write_stacking_save(root)
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
            gump_fallback_probe=args.gump_fallback_probe,
            script_item_type_reference=args.script_item_type_reference,
        )
    if args.timer_lifetime_probe or args.timer_lifetime_item_first_probe:
        write_timer_lifetime_save(root)
    if args.memory_timer_probe:
        write_memory_timer_save(root)
    if args.timer_default_remove_probe:
        write_timer_default_remove_save(root)
    if args.book_pages_probe:
        write_book_pages_save(root)
    if args.dword_hex_probe:
        write_dword_hex_save(root)
    if args.isbit_probe:
        write_isbit_save(root)
    if args.food_probe:
        write_food_save(root)
    if args.damage_trigger_probe:
        write_damage_trigger_save(root)
    if args.events_method_probe:
        write_events_method_save(root)
    if args.region_weather_probe:
        write_region_weather_save(root)
    if args.spawn_gem_probe:
        write_spawn_gem_save(
            root,
            duplicate_serials=args.spawn_gem_duplicate_serial_probe,
        )
    if args.spawn_point_probe:
        write_spawn_point_save(root)
    if args.escape_overflow_probe or args.daily_logging_probe:
        write_escape_overflow_save(root)
    if args.gm_command_log_probe:
        write_gm_command_log_save(root)
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
        extra_item_id=max(
            0x0E8A
            if (
                args.timer_sibling_mutation_probe
                or args.timer_sibling_mutation_owner_first_probe
                or args.container_shutdown_probe
            )
            else 0,
            max(CHARACTER_CONTENT_LAYERED_ITEM_ID, CHARACTER_CONTENT_SPECIAL_ITEM_ID)
            if args.character_content_probe
            else 0,
            max(STACKING_ITEM_ID, STACKING_NO_POINT_ITEM_ID)
            if args.movement_stacking_probe
            else 0,
            DAMAGE_TRIGGER_ITEM_ID if args.damage_trigger_probe else 0,
            OBJECT_ROOT_DISPATCH_ITEM_ID if args.object_root_dispatch_probe else 0,
            max(GUMP_FALLBACK_ITEM_ID, GUMP_FALLBACK_CHILD_ITEM_ID)
            if args.gump_fallback_probe
            else 0,
        ),
    )
    if args.timer_sibling_mutation_probe or args.timer_sibling_mutation_owner_first_probe:
        for item_id in (0x0E7D, 0x0E81, 0x0E86, 0x0E88, 0x0E89, 0x0E8A):
            write_container_tile(root / "muls" / "tiledata.mul", item_id)
    if args.container_shutdown_probe:
        for item_id in (0x0E7D, 0x0E88):
            write_container_tile(root / "muls" / "tiledata.mul", item_id)
    if args.character_content_probe:
        write_equipment_tile(
            root / "muls" / "tiledata.mul",
            CHARACTER_CONTENT_LAYERED_ITEM_ID,
            CHARACTER_CONTENT_LAYERED_ITEM_LAYER,
        )
        write_equipment_tile(
            root / "muls" / "tiledata.mul",
            CHARACTER_CONTENT_SPECIAL_ITEM_ID,
            CHARACTER_CONTENT_SPECIAL_ITEM_LAYER,
        )
    if args.movement_stairs_probe:
        tiledata = root / "muls" / "tiledata.mul"
        write_movement_tile(
            tiledata,
            MOVEMENT_STAIRS_ID,
            0x00000200 | 0x00000400 | 0x40000000,
            10,
        )
    if args.movement_stacking_probe:
        write_stackable_tile(root / "muls" / "tiledata.mul", STACKING_ITEM_ID)
        write_stackable_tile(root / "muls" / "tiledata.mul", STACKING_NO_POINT_ITEM_ID)
    if args.gump_fallback_probe:
        write_container_tile(root / "muls" / "tiledata.mul", GUMP_FALLBACK_ITEM_ID)
    print(f"wrote synthetic Sphere runtime fixture to {root}")
    return 0
