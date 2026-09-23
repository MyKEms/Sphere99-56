#!/usr/bin/env python3
"""Check dotted reference expressions and commands in a synthetic fixture.

Generate the fixture with ``make_fixture.py --dotted-expression-probe
--unknown-keyword-report``.  The probe evaluates every row of
``DOTTED_EXPRESSION_ROWS`` in a character trigger and in an item trigger,
runs a set of reference commands, and evaluates the numeric conditions of
``DOTTED_CONDITION_ROWS`` (bare reference operands, parentheses, unary !,
>= and <=, && and ||).  This test compares the reported values with the
expectations below and checks the unknown-keyword report for keys that only
a misparsed expression would produce.  The fixture's @EnvironChange handler
writes SECTOR.LIGHT behind a SECTOR.LIGHT guard; the test bounds how often
and how deeply that handler runs.
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
    DOTTED_CONDITION_ROWS,
    DOTTED_PROBE_CAPPED_FOR,
    DOTTED_PROBE_CAPPED_WHILE,
    DOTTED_EXPRESSION_ROWS,
    DOTTED_PROBE_ACCOUNT,
    DOTTED_PROBE_MARKER,
    DOTTED_PROBE_SECTOR_LIGHT,
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


def positive_at_most(limit: int) -> Check:
    def check(value: str, _values: dict[str, str]) -> Optional[str]:
        if value.isdigit() and 1 <= int(value) <= limit:
            return None
        return f"expected 1 to {limit}"

    return check


def sum_of(*keys: str, offset: int = 0) -> Check:
    def check(value: str, values: dict[str, str]) -> Optional[str]:
        parts = [values.get(key) for key in keys]
        if any(part is None or not part.lstrip("-").isdigit() for part in parts):
            return f"reference rows {keys} are missing"
        expected = str(sum(int(part) for part in parts) + offset)
        return None if value == expected else f"expected {expected!r}"

    return check


def negated(key: str) -> Check:
    def check(value: str, values: dict[str, str]) -> Optional[str]:
        base = values.get(key)
        if base is None or not base.isdigit():
            return f"reference row {key} is missing"
        expected = str(-int(base))
        return None if value == expected else f"expected {expected!r}"

    return check


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
    "C|src_sector_light": str(DOTTED_PROBE_SECTOR_LIGHT),
    "C|deferred_finduid_name": DOTTED_PROBE_ACCOUNT,
    "C|deferred_findlayer_serial": Same("I|serial"),
    # Chains rooted at a reference property of the default object, and
    # dotted TAG.name / TAG0.name reads.
    "C|sector_light": str(DOTTED_PROBE_SECTOR_LIGHT),
    "I|cont_name": DOTTED_PROBE_ACCOUNT,
    "I|cont_tag": "chartext",
    "I|topobj_name": DOTTED_PROBE_ACCOUNT,
    "C|tag_dot": "chartext",
    "I|tag_dot": "itemtext",
    "C|src_tag_dot": "chartext",
    "I|src_tag_dot": "chartext",
    "C|src_tag_dot_missing": "",
    "C|src_tag0_dot_missing": "0",
    "C|src_tag0_dot_num": "7",
    # Bare reference operands in EVAL.
    "C|eval_bare": sum_of("C|src_str", offset=1),
    "C|eval_bare_mixed": Same("C|eval_bracket_str_dex"),
    "C|eval_bracket_str_dex": number,
    "C|eval_bare_negative": negated("C|src_str"),
    "C|eval_bare_tag": "14",
    "I|eval_bare_tag": "14",
    "I|eval_bare_function": Same("C|function_plain"),
    "C|eval_defname": "1234",
    "C|eval_unknown_reference": "0",
    # Bare reference operands in IF conditions.
    "C|cond_src_str_eq": "1",
    "C|cond_src_str_gt": "1",
    "C|cond_src_str_lt": "0",
    "C|cond_chain_arith": "1",
    "C|cond_sector_light": "1",
    "C|cond_tag_set": "1",
    "C|cond_tag_paren": "1",
    "C|cond_tag_unset": "0",
    "C|cond_tag0_unset": "1",
    "C|cond_base_tag": "1",
    "C|cond_findlayer": "1",
    "C|cond_findlayer_empty": "0",
    "C|cond_finduid_name": "1",
    # Non-numeric values compare the way a substituted <...> value does.
    "C|cond_bare_name_other": Same("C|cond_bracket_name_other"),
    "C|cond_defname": "1",
    "C|cond_unknown_reference": "0",
    "C|cond_bare_and": "1",
    "C|cond_bare_paren": "1",
    "C|cond_bare_not": "1",
    # Expression grammar: parentheses, unary !, >= and <=, && above ||.
    "C|eval_paren_group": "9",
    "C|eval_paren_right": "14",
    "C|eval_not": "1",
    "C|eval_and": "1",
    "C|eval_or_false": "0",
    "C|eval_chain_left": "5",
    "C|eval_chain_no_precedence": "9",
    "C|eval_chain_compare": "2",
    "C|grammar_and_true": "1",
    "C|grammar_and_false": "0",
    "C|grammar_or_true": "1",
    "C|grammar_or_false": "0",
    "C|grammar_not_zero": "1",
    "C|grammar_not_one": "0",
    "C|grammar_not_paren": "1",
    "C|grammar_paren_arith": "1",
    "C|grammar_and_or": "1",
    "C|grammar_or_and": "1",
    "C|grammar_ge": "1",
    "C|grammar_ge_equal": "1",
    "C|grammar_ge_false": "0",
    "C|grammar_le_equal": "1",
    "C|grammar_le_false": "0",
    "C|grammar_unparenthesized": "1",
    "C|grammar_escape_terms": "1",
    "C|grammar_bare_terms": "1",
    "C|grammar_bare_terms_false": "0",
    "C|grammar_not_bare_set": "0",
    "C|grammar_and_operand_count": "1",
    "C|grammar_or_operand_count": "1",
    "C|grammar_escape_operand_count": "1",
    "C|grammar_elif": "elif",
    "C|grammar_while": "3",
    "C|cond_function_root_count": "1",
    "C|cond_function_call_count": "1",
    "C|while_bare_reference": "3",
    # Commands: base TAG, SRC.TAG, F_FUNC.TAG, FINDUID(uid).TAG, and the
    # item trigger's SRC.TAG, then SRC.NAME= and F_FUNC.REMOVE.
    "C|cmd_src_method": "reached",
    "C|cmd_readback": "23|21|30|41|11",
    "I|cmd_readback": "18",
    "C|cmd_src_name_set": "DottedRenamed",
    "C|disposable_before": "1",
    "C|disposable_after": "0",
    # Statements written as calls run once each with their arguments.
    "C|call_count": "4",
    "C|call_log": "start[3,4][5, 6][{src_str}][3]",
    "C|builtin_call": "reached",
    "C|call_readback": "9|8|7|12",
    "C|capped_loops_returned": "yes",
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
    # The @EnvironChange handler sets its sector light behind a guard that
    # never matches.  Entering the world runs it once; its own write may run
    # it once more, nested, but a write of the level already in effect must
    # not re-enter it again.
    "C|environ_calls": positive_at_most(2),
    "C|environ_max_depth": positive_at_most(2),
}

# Rows reported but deliberately not asserted: VAR.name reads are not
# implemented yet, getter_unknown/getter_malformed stay literal,
# dupe_reference prints an object reference, and cond_bracket_name_other is
# the reference value for cond_bare_name_other.
NOT_ASSERTED = {
    "C|unresolved_var_dot",
    "C|cond_bracket_name_other",
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
    keys |= {f"C|{key}" for key, _condition in DOTTED_CONDITION_ROWS}
    return keys | set(EXPECTED) | NOT_ASSERTED


def check_rows(values: dict[str, str]) -> tuple[list[str], int]:
    failures = []
    passed = 0
    for key, expectation in EXPECTED.items():
        if key not in values:
            failures.append(f"{key}: no value reported")
            continue
        value = values[key]
        if isinstance(expectation, str) and "{src_str}" in expectation:
            expectation = expectation.replace("{src_str}", values.get("C|src_str", "?"))
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


LOOP_LIMIT_RE = re.compile(r"(\S+)\((\d+)\): (WHILE|FOR) loop stopped after (\d+) iterations")


def loop_limit_failures(fixture: Path, log_contents: str) -> list[str]:
    """Each probe loop that reaches the limit is logged exactly once."""

    lines = (fixture / "scripts" / "spheretables.scp").read_text(encoding="ascii").splitlines()
    expected = {}
    for kind, text in (("WHILE", DOTTED_PROBE_CAPPED_WHILE), ("FOR", DOTTED_PROBE_CAPPED_FOR)):
        numbers = [index + 1 for index, line in enumerate(lines) if line.strip() == text]
        if len(numbers) != 1:
            return [f"fixture has {len(numbers)} '{text}' lines; expected 1"]
        expected[kind] = numbers[0]

    reported = LOOP_LIMIT_RE.findall(log_contents)
    failures = []
    for kind, line in expected.items():
        matches = [entry for entry in reported if entry[2] == kind]
        if len(matches) != 1:
            failures.append(f"{kind} loop limit logged {len(matches)} times; expected once")
            continue
        source, number, _kind, iterations = matches[0]
        if source != "spheretables.scp" or int(number) != line or iterations != "10000":
            failures.append(
                f"{kind} loop limit logged as {source}({number}) after {iterations}; "
                f"expected spheretables.scp({line}) after 10000"
            )
    return failures


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
    loop_failures = loop_limit_failures(fixture, log_contents)
    failures.extend(loop_failures)
    if not loop_failures:
        passed += 1

    total = len(EXPECTED) + 2
    if failures:
        print(f"dotted-expression probe failed: {passed}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dotted-expression probe passed: {passed}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
