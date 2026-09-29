#!/usr/bin/env python3
"""Create a small, redistributable runtime fixture for protocol tests.

The generated files are synthetic and are written to the caller-provided
directory.  Nothing from a client installation, shard runtime, world save, or
account database is read or copied.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from fixture_cases import FIXTURE_MODES
from modes.compat_writer import *  # noqa: F401,F403 - legacy API compatibility


def build_parser() -> argparse.ArgumentParser:
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
        "--timer-default-remove-probe",
        action="store_true",
        help="exercise an @Timer handler that removes its item without RETURN",
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
        "--isbit-probe",
        action="store_true",
        help="exercise the 0.99 ISBIT bit-position function",
    )
    parser.add_argument(
        "--food-probe",
        action="store_true",
        help="exercise the character FOOD property and item-event default object",
    )
    parser.add_argument(
        "--damage-trigger-probe",
        action="store_true",
        help="exercise item @Damage and source-character @ItemDamage dispatch",
    )
    parser.add_argument(
        "--events-method-probe",
        action="store_true",
        help="exercise the bare EVENTS(...) add/remove method",
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
        "--dialog-argv-probe",
        action="store_true",
        help="open a dialog with positional values consumed by its layout through ARGV",
    )
    parser.add_argument(
        "--dialog-argo-tag-probe",
        action="store_true",
        help="store a command with ARGO.TAG(name,value) and dispatch it from a button",
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
        "--daily-logging-probe",
        action="store_true",
        help="exercise daily logging for script, connection, and login messages",
    )
    parser.add_argument(
        "--runaway-loop-probe",
        action="store_true",
        help="exercise a configurable bounded WHILE loop and a second login",
    )
    parser.add_argument(
        "--recursion-depth-probe",
        action="store_true",
        help="exercise bounded recursive function and trigger calls",
    )
    parser.add_argument(
        "--movement-stairs-probe",
        action="store_true",
        help="exercise dynamic stair height resolution",
    )
    parser.add_argument(
        "--movement-stacking-probe",
        action="store_true",
        help="exercise same-definition stacking at explicit and no-point locations",
    )
    parser.add_argument(
        "--gump-fallback-probe",
        action="store_true",
        help="load a container with no TDATA2 gump and round-trip its child",
    )
    return parser



def generate_registered_mode(output: Path, mode) -> int:
    """Generate one auto-discovered mode using its private recipe.

    Mode modules call this lazily through ``modes.legacy_generator``.  Keeping
    the compatibility writer behind this adapter lets the command-line entry
    point remain a dispatcher while legacy flags continue to work for callers
    outside the manifest.
    """

    if mode.fixture_args is None:
        raise ValueError(f"mode {mode.name} has no make_fixture recipe")
    parser = build_parser()
    args = parser.parse_args([str(output), *mode.fixture_args])
    return generate_fixture(args, parser)


def main() -> int:
    parser = build_parser()
    args = parser.parse_args()
    if args.mode:
        from modes import generator_for

        return generator_for(args.mode)(args.output)
    return generate_fixture(args, parser)


if __name__ == "__main__":
    raise SystemExit(main())
