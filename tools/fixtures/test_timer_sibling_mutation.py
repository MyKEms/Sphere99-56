#!/usr/bin/env python3
"""Exercise sibling delete/reparent callbacks and verify deferred cleanup."""

from __future__ import annotations

import argparse
import re
import socket
import sys
import time
from pathlib import Path

from run_suite import shutdown_failures


ACCOUNT_NAME = "TimerSiblingMutationListener"
LOGIN_VALUE = "timer-sibling-mutation-pw"
UID_F_ITEM = 0x40000000

OWNER_UIDS = (200, 201, 202)
DEST_UIDS = tuple(UID_F_ITEM | serial for serial in (210, 211, 212))
KEEP_UIDS = tuple(UID_F_ITEM | serial for serial in (220, 221, 222))
A_UIDS = tuple(UID_F_ITEM | serial for serial in (230, 240, 250))
B_UIDS = tuple(UID_F_ITEM | serial for serial in (231, 241, 251))
C_UIDS = tuple(UID_F_ITEM | serial for serial in (232, 242, 253))
C_CHILD_UIDS = tuple(UID_F_ITEM | serial for serial in (233, 243, 254))

UID_ORDER = (*OWNER_UIDS, *DEST_UIDS, *KEEP_UIDS, *A_UIDS, *B_UIDS, *C_UIDS, *C_CHILD_UIDS)
BEFORE_UIDS = (1,) * len(UID_ORDER)
AFTER_UIDS = (
    (0, 0, 0)
    + (1, 1, 1)
    + (1, 1, 1)
    + (0, 0, 0)
    + (0, 1, 1)
    + (0, 0, 0)
    + (0, 0, 0)
)

REQUIRED_MARKERS = (
    "SPHERE_MUTATION_LISTENER_ALIVE",
    "SPHERE_MUTATION_UIDS_BEFORE",
    "SPHERE_MUTATION_UIDS_AFTER",
    "SPHERE_MUT_CASE1_TIMER",
    "SPHERE_MUT_CASE1_TIMER_RETURNED",
    "SPHERE_MUT_CASE1_CALLBACK",
    "SPHERE_MUT_CASE1_B_REMOVE_RETURNED",
    "SPHERE_MUT_CASE1_OWNER_REMOVE_RETURNED",
    "SPHERE_MUT_CASE2_TIMER",
    "SPHERE_MUT_CASE2_TIMER_RETURNED",
    "SPHERE_MUT_CASE2_CALLBACK",
    "SPHERE_MUT_CASE2_B_REPARENT_RETURNED",
    "SPHERE_MUT_CASE2_OWNER_REMOVE_RETURNED",
    "SPHERE_MUT_CASE3_TIMER",
    "SPHERE_MUT_CASE3_TIMER_RETURNED",
    "SPHERE_MUT_CASE3_CALLBACK",
    "SPHERE_MUT_CASE3_B_REPARENT_RETURNED",
    "SPHERE_MUT_CASE3_OWNER_REMOVE_RETURNED",
    "SPHERE_MUT_CASE2_B_PARENT",
    "SPHERE_MUT_CASE3_B_PARENT",
    "SPHERE_MUT_CASE1_KEEP_PARENT",
    "SPHERE_MUT_CASE2_KEEP_PARENT",
    "SPHERE_MUT_CASE3_KEEP_PARENT",
    "SPHERE_REVIEW_UNKNOWN",
    "SPHERE_REVIEW_UNKNOWN_COUNT",
    "SPHERE_REVIEW_MALFORMED",
    "SPHERE_REVIEW_MALFORMED_COUNT",
    "SPHERE_REVIEW_REFERENCE",
    "SPHERE_REVIEW_REFERENCE_COUNT",
    "SPHERE_REVIEW_DUPE_BEFORE",
    "SPHERE_REVIEW_DUPE_REF",
    "SPHERE_REVIEW_DUPE_REF_COUNT",
    "SPHERE_REVIEW_DUPE_VALUE",
    "SPHERE_REVIEW_DUPE_VALUE_COUNT",
    "SPHERE_REVIEW_DUPE_VALUE_VALID",
    "SPHERE_REVIEW_DUPE_VALUE_VALID_COUNT",
    "SPHERE_REVIEW_DUPE_AFTER",
)


def all_system_messages(data: bytes) -> list[str]:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    messages = []
    for packet in split_packet_stream(decode_game_response(data), allow_truncated=True):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
    return messages


def marker_count(messages: list[str], prefix: str) -> int:
    return sum(message == prefix or message.startswith(prefix + " ") for message in messages)


def collect_markers(
    sock: socket.socket,
    initial: bytes,
    timeout: float,
) -> bytes:
    from uo_packets import split_packet_stream
    from uo_test_client import decode_game_response

    data = bytearray(initial)
    deadline = time.monotonic() + timeout
    sock.settimeout(0.2)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        except (ConnectionResetError, OSError):
            break
        if not chunk:
            break
        data.extend(chunk)
        decoded = decode_game_response(bytes(data))
        messages = []
        for packet in split_packet_stream(decoded, allow_truncated=True):
            if packet.command == 0x1C and len(packet.data) >= 45:
                messages.append(packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace"))
        if all(
            any(message == prefix or message.startswith(prefix + " ") for message in messages)
            for prefix in REQUIRED_MARKERS
        ):
            break
    return bytes(data)


def parse_bits(messages: list[str], prefix: str) -> tuple[int, ...] | None:
    marker = next((message for message in messages if message.startswith(prefix)), None)
    if marker is None:
        return None
    expected = len(UID_ORDER)
    match = re.fullmatch(rf"{re.escape(prefix)} ([01](?:\|[01]){{{expected - 1}}})", marker)
    return tuple(int(value) for value in match.group(1).split("|")) if match else None


def parse_value(messages: list[str], prefix: str) -> int | None:
    marker = next((message for message in messages if message.startswith(prefix)), None)
    if marker is None:
        return None
    match = re.fullmatch(rf"{re.escape(prefix)} ([0-9A-Fa-fx]+)", marker)
    if not match:
        return None
    token = match.group(1)
    try:
        return int(token, 0)
    except ValueError:
        return int(token, 16)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2796)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
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
    evidence: dict[str, object] = {}

    def exercise() -> None:
        sock, _ = game_connect(
            args.host,
            args.port,
            ACCOUNT_NAME,
            LOGIN_VALUE,
            game_port=args.port + 1000,
        )
        if sock is None:
            raise RuntimeError("listener did not reach the character list")
        try:
            sock.sendall(
                make_char_create(
                    name=ACCOUNT_NAME,
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
                raise RuntimeError("listener did not enter the world")
            messages = collect_markers(sock, response, 28.0)
            observed = all_system_messages(messages)

            before = parse_bits(observed, "SPHERE_MUTATION_UIDS_BEFORE")
            after = parse_bits(observed, "SPHERE_MUTATION_UIDS_AFTER")
            if before != BEFORE_UIDS:
                failures.append(f"mutation fixture UIDs were incomplete before teardown: {before!r}")
            if after != AFTER_UIDS:
                failures.append(f"mutation fixture UIDs/cleanup differed after teardown: {after!r}")

            expected_relations = {
                "SPHERE_MUT_CASE2_B_PARENT": DEST_UIDS[1],
                "SPHERE_MUT_CASE3_B_PARENT": DEST_UIDS[2],
                "SPHERE_MUT_CASE1_KEEP_PARENT": DEST_UIDS[0],
                "SPHERE_MUT_CASE2_KEEP_PARENT": DEST_UIDS[1],
                "SPHERE_MUT_CASE3_KEEP_PARENT": DEST_UIDS[2],
            }
            for prefix, expected in expected_relations.items():
                actual = parse_value(observed, prefix)
                if actual != expected:
                    failures.append(f"{prefix} expected parent {expected}, got {actual!r}")

            for prefix in (
                "SPHERE_REVIEW_UNKNOWN_COUNT",
                "SPHERE_REVIEW_MALFORMED_COUNT",
                "SPHERE_REVIEW_REFERENCE_COUNT",
                "SPHERE_REVIEW_DUPE_REF_COUNT",
                "SPHERE_REVIEW_DUPE_VALUE_COUNT",
                "SPHERE_REVIEW_DUPE_VALUE_VALID_COUNT",
            ):
                actual = parse_value(observed, prefix)
                if actual != 1:
                    failures.append(f"{prefix} expected getter count 1, got {actual!r}")

            dupe_before = parse_value(observed, "SPHERE_REVIEW_DUPE_BEFORE")
            dupe_after = parse_value(observed, "SPHERE_REVIEW_DUPE_AFTER")
            dupe_value = parse_value(observed, "SPHERE_REVIEW_DUPE_VALUE")
            dupe_valid = parse_value(observed, "SPHERE_REVIEW_DUPE_VALUE_VALID")
            if dupe_before is None or dupe_after is None:
                failures.append(
                    f"DUPE probe did not report character counts: "
                    f"before={dupe_before!r}, after={dupe_after!r}"
                )
            elif dupe_after - dupe_before != 3:
                failures.append(
                    f"reference-returning DUPE suffix created "
                    f"{dupe_after - dupe_before} characters; expected 3"
                )
            if dupe_value is None or dupe_value <= 0:
                failures.append(f"DUPE suffix returned an invalid serial: {dupe_value!r}")
            if dupe_valid != 1:
                failures.append(f"DUPE suffix serial did not resolve to a live UID: {dupe_valid!r}")

            for prefix in REQUIRED_MARKERS:
                count = marker_count(observed, prefix)
                if count != 1:
                    failures.append(f"mutation marker {prefix} occurred {count} times")
            evidence.update(
                before_uids=before,
                after_uids=after,
                relation_values={
                    prefix: parse_value(observed, prefix)
                    for prefix in expected_relations
                },
                dotted_dupe={
                    "before_chars": dupe_before,
                    "after_chars": dupe_after,
                    "reference_count": parse_value(observed, "SPHERE_REVIEW_DUPE_REF_COUNT"),
                    "value": dupe_value,
                    "value_count": parse_value(observed, "SPHERE_REVIEW_DUPE_VALUE_COUNT"),
                    "value_valid": dupe_valid,
                    "value_valid_count": parse_value(
                        observed, "SPHERE_REVIEW_DUPE_VALUE_VALID_COUNT"
                    ),
                },
                marker_counts={prefix: marker_count(observed, prefix) for prefix in REQUIRED_MARKERS},
            )
            if failures:
                failures.append(f"observed synthetic messages: {observed!r}")
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
    if failures:
        print("timer-sibling-mutation probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(
        "timer-sibling-mutation probe passed: "
        f"before_uids={evidence['before_uids']!r} "
        f"after_uids={evidence['after_uids']!r} "
        f"relations={evidence['relation_values']!r} "
        f"dotted_dupe={evidence['dotted_dupe']!r}"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
