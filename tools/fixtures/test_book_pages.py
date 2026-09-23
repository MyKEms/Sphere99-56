#!/usr/bin/env python3
"""Load and read a BOOK past the 7-bit page field and a full resource-ID ITEMDEF.

The fixture comes from ``make_fixture.py --book-pages-probe``.  The load check
requires every BOOK page section and the ITEMDEF named by a complete resource
ID to load without an exception, the out-of-range page to be rejected with a
message, and the ITEMDEF to select the entry its index names.  The protocol
check opens the book as a client and reads pages on both sides of the page
field limit.
"""

from __future__ import annotations

import argparse
import re
import socket
import subprocess
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from make_fixture import (
    BOOK_PROBE_ACCOUNT,
    BOOK_PROBE_ITEM_SERIAL,
    BOOK_PROBE_NAME,
    BOOK_PROBE_PAGES,
    BOOK_PROBE_REJECTED_PAGE,
    BOOK_READ_NAME,
    BOOK_READ_PAGES,
    BOOK_READ_TITLE,
    FULL_RID_ALIAS,
    UID_F_ITEM,
    book_page_lines,
)
from run_suite import shutdown_failures


LOGIN_VALUE = "book-probe-pw"
CLIENT_VERSION = b"3.0.6"
BOOK_UID = UID_F_ITEM | BOOK_PROBE_ITEM_SERIAL
DEFNAME_RE = re.compile(r"^([A-Za-z0-9_]+)=(-?\d+)$")
EXPECTED_LOAD_ERRORS = (
    f"Bad resource index page {BOOK_PROBE_REJECTED_PAGE}",
    f"Invalid BOOK block index '{BOOK_PROBE_NAME} {BOOK_PROBE_REJECTED_PAGE}'",
)


def load_failures(binary: Path, fixture: Path) -> list[str]:
    result = subprocess.run(
        [str(binary), "-D1", "-Q"],
        cwd=fixture,
        check=False,
        capture_output=True,
        text=True,
        errors="replace",
        timeout=120,
    )
    output = result.stdout + result.stderr
    failures = []
    if result.returncode != 255:
        failures.append(f"load did not exit through -Q (status {result.returncode})")

    exceptions = [line for line in output.splitlines() if "Exception loading section" in line]
    failures.extend(f"load exception: {line}" for line in exceptions[:10])
    if len(exceptions) > 10:
        failures.append(f"... {len(exceptions) - 10} more load exception line(s)")
    for message in EXPECTED_LOAD_ERRORS:
        if message not in output:
            failures.append(f"load did not report: {message}")

    dump_path = fixture / "dumpdefs.txt"
    if not dump_path.is_file():
        failures.append("-D1 did not write dumpdefs.txt")
        return failures
    names = {}
    for line in dump_path.read_text(encoding="ascii", errors="replace").splitlines():
        match = DEFNAME_RE.fullmatch(line)
        if match:
            names[match.group(1)] = int(match.group(2))
    for name in (BOOK_PROBE_NAME, BOOK_READ_NAME, "SYNTHETIC_OBJECT", FULL_RID_ALIAS):
        if name not in names:
            failures.append(f"resource dump is missing {name}")
    if (
        "SYNTHETIC_OBJECT" in names
        and FULL_RID_ALIAS in names
        and names["SYNTHETIC_OBJECT"] != names[FULL_RID_ALIAS]
    ):
        failures.append(
            f"{FULL_RID_ALIAS} names resource {names[FULL_RID_ALIAS] & 0xFFFFFFFF:#010x}, "
            f"not SYNTHETIC_OBJECT {names['SYNTHETIC_OBJECT'] & 0xFFFFFFFF:#010x}"
        )
    return failures


def client_version() -> bytes:
    text = CLIENT_VERSION + b"\0"
    return bytes([0xBD]) + (3 + len(text)).to_bytes(2, "big") + text


def double_click(uid: int) -> bytes:
    return bytes([0x06]) + uid.to_bytes(4, "big")


def page_request(uid: int, page: int) -> bytes:
    return (
        bytes([0x66])
        + (13).to_bytes(2, "big")
        + uid.to_bytes(4, "big")
        + (1).to_bytes(2, "big")
        + page.to_bytes(2, "big")
        + (0xFFFF).to_bytes(2, "big")
    )


def parse_page(data: bytes) -> Optional[tuple[int, list[str]]]:
    """Return (page, lines) of a one-page 0x66 packet for the probe book."""

    if len(data) < 13 or int.from_bytes(data[3:7], "big") != BOOK_UID:
        return None
    page = int.from_bytes(data[9:11], "big")
    count = int.from_bytes(data[11:13], "big")
    lines = [
        raw.decode("latin-1") for raw in data[13:].split(b"\0")[:count]
    ]
    while lines and not lines[-1].strip():
        lines.pop()
    return page, [line.rstrip("\r") for line in lines]


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str]) -> None:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    # pylint: disable=import-outside-toplevel
    from uo_packets import split_packet_stream
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    sock, _ = game_connect(
        args.host, args.port, BOOK_PROBE_ACCOUNT, LOGIN_VALUE, game_port=args.port + 1000
    )
    if sock is None:
        raise RuntimeError("probe account did not reach the character list")
    try:
        sock.sendall(
            make_char_create(
                name=BOOK_PROBE_ACCOUNT,
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
        raw = bytearray(recv_until_game_start(sock, timeout=30.0))
        if find_start_packet(decode_game_response(bytes(raw))) is None:
            raise RuntimeError("probe character did not enter the world")
        seen = len(split_packet_stream(decode_game_response(bytes(raw)), allow_truncated=True))

        def wait_for(match: Callable[[bytes], bool], timeout: float = 10.0) -> Optional[bytes]:
            nonlocal seen
            deadline = time.monotonic() + timeout
            sock.settimeout(0.2)
            while True:
                packets = split_packet_stream(
                    decode_game_response(bytes(raw)), allow_truncated=True
                )
                fresh, seen = packets[seen:], len(packets)
                for packet in fresh:
                    if match(packet.data):
                        return packet.data
                if time.monotonic() >= deadline:
                    return None
                try:
                    chunk = sock.recv(65536)
                except socket.timeout:
                    continue
                if not chunk:
                    return None
                raw.extend(chunk)

        # The book header layout follows the client version.
        sock.sendall(client_version())
        sock.sendall(double_click(BOOK_UID))
        header = wait_for(
            lambda data: data[0] == 0x93 and int.from_bytes(data[1:5], "big") == BOOK_UID
        )
        if header is None:
            failures.append("opening the book returned no 0x93 book header")
            return
        pages = int.from_bytes(header[7:9], "big")
        title = header[9:69].split(b"\0")[0].decode("latin-1")
        if pages != BOOK_PROBE_PAGES or title != BOOK_READ_TITLE:
            failures.append(f"book header has {pages} pages, title {title!r}")
        else:
            passed.append("book header")

        def read_page(page: int) -> Optional[list[str]]:
            sock.sendall(page_request(BOOK_UID, page))
            data = wait_for(
                lambda data: data[0] == 0x66 and (parse_page(data) or (0,))[0] == page
            )
            return None if data is None else parse_page(data)[1]

        for page in BOOK_READ_PAGES:
            lines = read_page(page)
            if lines is None:
                failures.append(f"page {page} was not returned")
            elif lines != book_page_lines(page):
                failures.append(f"page {page} returned {lines!r}")
            else:
                passed.append(f"page {page}")

        # A page number past the limit is ignored and the book still reads.
        sock.sendall(page_request(BOOK_UID, BOOK_PROBE_REJECTED_PAGE + 44))
        lines = read_page(1)
        if lines != book_page_lines(1):
            failures.append(f"page 1 after an out-of-range request returned {lines!r}")
        else:
            passed.append("out-of-range page request")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2732)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    parser.add_argument(
        "--load-only",
        action="store_true",
        help="run only the load check (no client session)",
    )
    args = parser.parse_args()

    args.fixture = args.fixture.resolve()
    args.binary = args.binary.resolve()
    if not (args.fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {args.fixture / 'sphere.ini'}")
    if not args.binary.is_file():
        parser.error(f"server binary does not exist: {args.binary}")

    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel

    failures = load_failures(args.binary, args.fixture)
    passed = [] if failures else ["load"]
    total = 1

    if not args.load_only:
        returncode, runner_error, log_contents = run_server(
            fixture=args.fixture,
            binary=args.binary,
            host=args.host,
            port=args.port,
            startup_timeout=args.startup_timeout,
            log_path=args.fixture / "server.log",
            action=lambda: exercise(args, failures, passed),
        )
        if runner_error:
            failures.append(runner_error)
        failures.extend(shutdown_failures(returncode, log_contents))
        total += 1 + len(BOOK_READ_PAGES) + 1

    if failures:
        print(f"book-pages probe failed: {len(passed)}/{total} checks passed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"book-pages probe passed: {len(passed)}/{total} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
