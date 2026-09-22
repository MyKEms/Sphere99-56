#!/usr/bin/env python3
"""Check dotted reference expressions and commands in a synthetic fixture.

Generate the fixture with ``make_fixture.py --dotted-expression-probe
--unknown-keyword-report``.  The probe evaluates every row of
``DOTTED_EXPRESSION_ROWS`` in a character trigger and in an item trigger and
runs a set of reference commands; this test compares the reported values with
the expectations below and checks the unknown-keyword report for keys that only
a misparsed expression would produce.
"""

from __future__ import annotations

import argparse
import json
import re
import socket
import sys
import time
from pathlib import Path
from typing import Callable, Optional, Union

from make_fixture import (
    DOTTED_EXPRESSION_ROWS,
    DOTTED_PROBE_ACCOUNT,
    DOTTED_PROBE_MARKER,
)
from run_suite import shutdown_failures


LOGIN_VALUE = "dotted-probe-pw"
PROBE_ITEM_NAME = "synthetic dotted probe"
END_MARKER = DOTTED_PROBE_MARKER + "_END"
MARKER_RE = re.compile(re.escape(DOTTED_PROBE_MARKER) + r" ([CI])\|([a-z0-9_]+)\|\[(.*)\]$")


class Same:
    """Expect the same value as another probe row."""

    def __init__(self, key: str) -> None:
        self.key = key


Check = Callable[[str, dict[str, str]], Optional[str]]
Expectation = Union[str, Same, Check]


def number(value: str, _values: dict[str, str]) -> Optional[str]:
    return None if value.isdigit() and int(value) > 0 else "expected a positive number"


def nonempty(value: str, _values: dict[str, str]) -> Optional[str]:
    return None if value else "expected a value"


def decimal_times(key: str, factor: int) -> Check:
    def check(value: str, values: dict[str, str]) -> Optional[str]:
        base = values.get(key)
        if base is None or not base.isdigit():
            return f"reference row {key} is missing"
        expected = str(int(base) * factor)
        return None if value == expected else f"expected {expected!r}"

    return check


EXPECTED: dict[str, Expectation] = {
    # Forms without a function-root chain.
    "C|src_name": DOTTED_PROBE_ACCOUNT,
    "I|src_name": DOTTED_PROBE_ACCOUNT,
    "C|src_str": number,
    # The command rows address the player as FINDUID(1): the fixture world
    # starts empty, so the probe character is the first character created.
    "C|src_serial": "01",
    "I|src_serial": Same("C|src_serial"),
    "C|serv_name": "Sphere99 synthetic fixture",
    "C|var_paren": "globalvalue",
    "C|eval_decimal": "75",
    "I|eval_decimal": "75",
    "C|eval_decimal_zero": "300",
    "C|eval_paren_decimal": "25",
    "C|eval_nested": decimal_times("C|src_str", 15),
    "C|strlen_dot": "3",
    "I|strlen_dot": "3",
    "C|strcmp_dot": "0",
    "C|strindexof_dot": "4",
    "C|safe_src_name": DOTTED_PROBE_ACCOUNT,
    "C|safe_missing_tag": "",
    "C|tag_paren": "chartext",
    "I|tag_paren": "itemtext",
    "C|function_plain": number,
    "I|function_plain": number,
    "I|serial": nonempty,
    "C|deferred_src_tag": "chartext",
    "C|deferred_eval": "75",
    "C|deferred_strlen": "3",
    # One-level references: the segment keeps its own arguments, and a script
    # function segment runs with the reference as its default object.
    "C|src_tag_paren": "chartext",
    "I|src_tag_paren": "chartext",
    "C|src_tag_paren_num": "7",
    "C|src_tag_paren_missing": "",
    "C|src_function": Same("C|function_plain"),
    "I|src_function": Same("C|function_plain"),
    "C|src_function_args": "5",
    "C|function_root_tag": "chartext",
    "I|function_root_tag": "itemtext",
    "C|finduid_missing_name": "",
    # Function roots with arguments and multi-level chains.
    "C|src_account_name": DOTTED_PROBE_ACCOUNT,
    "I|src_account_name": DOTTED_PROBE_ACCOUNT,
    "C|findaccount_name": DOTTED_PROBE_ACCOUNT,
    "C|finduid_name": DOTTED_PROBE_ACCOUNT,
    "C|finduid_serial": Same("C|src_serial"),
    "C|finduid_tag": "chartext",
    "C|finduid_function": Same("C|function_plain"),
    "C|lastnewitem_name": PROBE_ITEM_NAME,
    "C|function_args_root": DOTTED_PROBE_ACCOUNT,
    "C|function_args_chain": Same("I|serial"),
    "C|src_findlayer_name": PROBE_ITEM_NAME,
    "I|src_findlayer_name": PROBE_ITEM_NAME,
    "C|src_findlayer_serial": Same("I|serial"),
    "I|src_findlayer_serial": Same("I|serial"),
    "C|src_findlayer_tag": "itemtext",
    "C|deferred_finduid_name": DOTTED_PROBE_ACCOUNT,
    "C|deferred_findlayer_serial": Same("I|serial"),
    # Commands: base TAG, SRC.TAG, F_FUNC.TAG, FINDUID(uid).TAG, and the
    # item trigger's SRC.TAG, then SRC.NAME= and F_FUNC.REMOVE.
    "C|cmd_src_method": "reached",
    "C|cmd_readback": "23|21|30|41|11",
    "I|cmd_readback": "18",
    "C|cmd_src_name_set": "DottedRenamed",
    "C|disposable_before": "1",
    "C|disposable_after": "0",
    # Exactly-once evaluation of a reference-returning function root.
    "C|getter_unknown_count": "1",
    "C|getter_malformed_count": "1",
    "C|getter_reference": DOTTED_PROBE_ACCOUNT,
    "C|getter_reference_count": "1",
    "C|dupe_reference_count": "1",
    "C|dupe_value": nonempty,
    "C|dupe_value_count": "1",
    "C|dupe_value_valid": "1",
    "C|dupe_value_valid_count": "1",
}

# Rows reported but deliberately not asserted: the unresolved_* rows are not
# implemented on any build yet, getter_unknown/getter_malformed stay literal,
# and dupe_reference prints an object reference.
NOT_ASSERTED = {
    "C|unresolved_src_tag_dot",
    "C|unresolved_var_dot",
    "C|getter_unknown",
    "C|getter_malformed",
    "C|dupe_reference",
    "C|dupe_chars_before",
    "C|dupe_chars_after",
}

# Unknown-keyword report keys that only appear when a dotted expression is
# split at the wrong place: a rejected bare TAG/VAR root (the normalized keys
# are TAG.* and VAR.*), or a function name that still carries its
# space-separated arguments ("EVAL 5*1").
BARE_REJECTED_ROOTS = {"TAG", "VAR"}


def misparsed_report_key(entry: dict) -> bool:
    keyword = str(entry.get("keyword", ""))
    if " " in keyword:
        return True
    return entry.get("kind") == "rejected" and keyword in BARE_REJECTED_ROOTS


def system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def parse_rows(messages: list[str]) -> tuple[dict[str, str], list[str]]:
    values: dict[str, str] = {}
    malformed = []
    for message in messages:
        if not message.startswith(DOTTED_PROBE_MARKER + " "):
            continue
        match = MARKER_RE.match(message)
        if match is None:
            malformed.append(message)
            continue
        values[f"{match.group(1)}|{match.group(2)}"] = match.group(3)
    return values, malformed


def expected_keys() -> set[str]:
    keys = {
        f"{context}|{key}"
        for key, _expression, contexts in DOTTED_EXPRESSION_ROWS
        for context in contexts
    }
    return keys | set(EXPECTED) | NOT_ASSERTED


def check_rows(values: dict[str, str]) -> tuple[list[str], int]:
    failures = []
    passed = 0
    for key, expectation in EXPECTED.items():
        if key not in values:
            failures.append(f"{key}: no value reported")
            continue
        value = values[key]
        if isinstance(expectation, Same):
            reference = values.get(expectation.key)
            error = None if reference is not None and value == reference else (
                f"expected the value of {expectation.key} ({reference!r})"
            )
        elif isinstance(expectation, str):
            error = None if value == expectation else f"expected {expectation!r}"
        else:
            error = expectation(value, values)
        if error:
            failures.append(f"{key}: got {value!r}, {error}")
        else:
            passed += 1

    before = values.get("C|dupe_chars_before", "")
    after = values.get("C|dupe_chars_after", "")
    if before.isdigit() and after.isdigit() and int(after) - int(before) == 3:
        passed += 1
    else:
        failures.append(
            f"DUPE suffixes changed the character count from {before!r} to {after!r}; "
            "expected exactly 3 new characters"
        )
    return failures, passed


def report_failures(report_path: Path) -> list[str]:
    deadline = time.monotonic() + 5.0
    while not report_path.is_file() and time.monotonic() < deadline:
        time.sleep(0.05)
    if not report_path.is_file():
        return [f"unknown-keyword report was not written: {report_path}"]
    report = json.loads(report_path.read_text(encoding="utf-8"))
    bogus = sorted(
        f"{entry.get('kind')}:{entry.get('keyword')}"
        for entry in report.get("entries", [])
        if misparsed_report_key(entry)
    )
    return [f"unknown-keyword report contains misparsed key {key}" for key in bogus]


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2730)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument("--dump", action="store_true", help="print every reported row")
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel
    from uo_test_client import (  # pylint: disable=import-outside-toplevel
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    failures: list[str] = []
    messages: list[str] = []

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            DOTTED_PROBE_ACCOUNT,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("probe account did not reach the character list")
        try:
            sock.sendall(
                make_char_create(
                    name=DOTTED_PROBE_ACCOUNT,
                    sex=0,
                    start_loc=1,
                    skill1=25,
                    val1=40,
                    skill2=26,
                    val2=40,
                    skill3=1,
                    val3=20,
                )
            )
            response = recv_until_game_start(sock, timeout=30.0)
            if not response or find_start_packet(decode_game_response(response)) is None:
                raise RuntimeError("probe character did not enter the world")
            data = bytearray(response)
            deadline = time.monotonic() + 30.0
            sock.settimeout(0.2)
            while time.monotonic() < deadline:
                if any(message == END_MARKER for message in system_messages(bytes(data))):
                    break
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                except OSError:
                    break
                if not chunk:
                    break
                data.extend(chunk)
            messages.extend(system_messages(bytes(data)))
        finally:
            sock.close()

    returncode, runner_error, log_contents = run_server(
        fixture=fixture,
        binary=binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=fixture / "server.log",
        action=exercise,
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(shutdown_failures(returncode, log_contents))

    values, malformed = parse_rows(messages)
    if args.dump:
        for key in sorted(values):
            print(f"{key} = {values[key]!r}")
    if END_MARKER not in messages:
        failures.append("probe did not reach its end marker")
    failures.extend(f"malformed probe line: {line!r}" for line in malformed)
    unexpected = sorted(set(values) - expected_keys())
    failures.extend(f"unexpected probe row {key}" for key in unexpected)
    row_failures, passed = check_rows(values)
    failures.extend(row_failures)
    failures.extend(report_failures(fixture / "logs" / "unknown-keywords.json"))

    total = len(EXPECTED) + 1
    if failures:
        print(f"dotted-expression probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dotted-expression probe passed: {passed}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
