#!/usr/bin/env python3
"""Check dialogs laid out by script functions called on the dialog object.

The fixture comes from ``make_fixture.py --mode dialog-root-dispatch``.  The
client double-clicks the fixture item, which opens a dialog on itself.  The
test decodes every 0xB0 packet strictly (declared length, layout terminator,
text count and each big-endian UTF-16 line) and checks:

* the first dialog holds the control written in its layout and every control
  and text built by ``argo.<function>(...)`` calls, including the ones those
  functions add through ARGO, through a nested call and in call form; a
  function whose name starts with a control name runs instead of becoming a
  control; and a text line longer than one conversion buffer arrives whole;
* the button reply runs a function on ARGO, which sees the TAG the layout
  functions set and opens a second dialog larger than one output buffer;
* the second dialog's button opens a dialog too large for one 0xB0 packet:
  it is refused, the handler continues, and nothing malformed is sent;
* a following walk request is acknowledged, so the stream stays in step;
* the unknown-keyword report names a missing function called on ARGO and has
  no unresolved ARGO entry, and the recursion guard ended a self-calling
  layout function.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from modes.dialog_root_dispatch import (
    ACCOUNT,
    FIRST_BUTTON,
    FIRST_CONTROLS,
    FIRST_TEXTS,
    ITEM_UID,
    MARKER,
    MISSING_FUNCTION,
    NEXT_BUTTON,
    NEXT_CONTROLS,
    NEXT_TEXTS,
    PANEL_Y,
    PASSWORD,
)
from run_suite import shutdown_failures

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from test_packet_burst import _decode_frame  # noqa: E402
from uo_gumps import GumpPacketError, make_gump_reply, parse_gump_dialog  # noqa: E402
from uo_packets import UO_PACKET_LENGTHS  # noqa: E402
from uo_test_client import (  # noqa: E402
    decode_game_response,
    find_start_packet,
    game_relogin,
    make_char_play,
    recv_until_game_start,
)

CHECKS = (
    "first dialog",
    "first dialog controls",
    "first dialog texts",
    "button handler",
    "second dialog",
    "second dialog controls",
    "second dialog texts",
    "oversize dialog refused",
    "walk acknowledged",
    "keyword report",
    "recursion guard",
)
OVERSIZE_LOG = "Gump dialog is too large to send"
RECURSION_LOG = "Trigger Recursion error"


def _system_message(packet: bytes) -> Optional[str]:
    if packet[0] != 0x1C or len(packet) < 45:
        return None
    return packet[44:].split(b"\0", 1)[0].decode("latin-1")


class Stream:
    """Decode one game connection incrementally, packet by packet.

    The whole compressed stream after the character list is kept, so a
    packet split across reads or compressed frames is still framed exactly.
    An unknown command or a short packet is a framing error.
    """

    def __init__(self, sock: socket.socket, raw: bytes) -> None:
        self.sock = sock
        self.raw = bytearray(raw)
        self.raw_pos = 0
        self.decoded = bytearray()
        self.decoded_pos = 0
        self.packets: list[bytes] = []
        self.closed = False
        self._consume()

    def _consume(self) -> None:
        while True:
            frame = _decode_frame(self.raw, self.raw_pos)
            if frame is None:
                break
            data, self.raw_pos = frame
            self.decoded.extend(data)
        while self.decoded_pos < len(self.decoded):
            command = self.decoded[self.decoded_pos]
            length = UO_PACKET_LENGTHS.get(command)
            if length is None:
                raise ValueError(f"unknown server packet 0x{command:02x}")
            if length < 0:
                if len(self.decoded) - self.decoded_pos < 3:
                    break
                length = int.from_bytes(
                    self.decoded[self.decoded_pos + 1:self.decoded_pos + 3], "big"
                )
                if length < 3:
                    raise ValueError(f"server packet 0x{command:02x} declares length {length}")
            if len(self.decoded) - self.decoded_pos < length:
                break
            self.packets.append(bytes(self.decoded[self.decoded_pos:self.decoded_pos + length]))
            self.decoded_pos += length

    def wait_for(
        self, match: Callable[[bytes], bool], start: int, timeout: float = 10.0
    ) -> Optional[int]:
        """Return the index of the first packet from *start* that matches."""

        deadline = time.monotonic() + timeout
        self.sock.settimeout(0.2)
        while True:
            for index in range(start, len(self.packets)):
                if match(self.packets[index]):
                    return index
            start = max(start, len(self.packets))
            if self.closed or time.monotonic() >= deadline:
                return None
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError:
                self.closed = True
                continue
            if not chunk:
                self.closed = True
                continue
            self.raw.extend(chunk)
            self._consume()


def _enter_world(args: argparse.Namespace) -> tuple[socket.socket, bytes]:
    for _attempt in range(10):
        sock, _, initial = game_relogin(
            args.host, args.port, ACCOUNT, PASSWORD, game_port=args.port + 1000
        )
        if sock is not None:
            break
        time.sleep(1.0)
    else:
        raise RuntimeError("probe account did not reach the character list")
    if find_start_packet(initial) is not None:
        sock.close()
        raise RuntimeError("probe character entered without a character list")
    sock.sendall(make_char_play(0))
    raw = recv_until_game_start(sock, timeout=15.0)
    if find_start_packet(decode_game_response(raw)) is None:
        sock.close()
        raise RuntimeError("probe character did not enter the world")
    return sock, raw


def _check_gump(
    name: str,
    packet: Optional[bytes],
    controls: tuple[str, ...],
    texts: tuple[str, ...],
    failures: list[str],
    passed: list[str],
    log: list[str],
):
    if packet is None:
        failures.append(f"{name}: no 0xB0 dialog arrived")
        return None
    try:
        gump = parse_gump_dialog(packet)
    except GumpPacketError as error:
        failures.append(f"{name}: malformed 0xB0: {error}")
        return None
    sent_controls = tuple(" ".join(control.split()) for control in gump.layout.strip("{}").split("}{"))
    log.append(
        f"{name}: serial=0x{gump.serial:08x} context={gump.context} "
        f"layout={gump.layout!r} texts={[text[:24] + ('...' if len(text) > 24 else '') for text in gump.texts]!r} "
        f"text_lengths={[len(text) for text in gump.texts]!r}"
    )
    if gump.serial != ITEM_UID:
        failures.append(f"{name}: dialog serial 0x{gump.serial:08x}, expected 0x{ITEM_UID:08x}")
    else:
        passed.append(name)
    if sent_controls != controls:
        failures.append(f"{name} controls: sent {list(sent_controls)!r}, expected {list(controls)!r}")
    else:
        passed.append(f"{name} controls")
    if gump.texts != texts:
        differing = [
            index
            for index in range(max(len(gump.texts), len(texts)))
            if index >= len(gump.texts) or index >= len(texts) or gump.texts[index] != texts[index]
        ]
        failures.append(
            f"{name} texts: {len(gump.texts)} lines sent, {len(texts)} expected; "
            f"lines differing: {differing!r}"
        )
    else:
        passed.append(f"{name} texts")
    return gump


def exercise(args: argparse.Namespace, failures: list[str], passed: list[str], log: list[str]) -> None:
    sock, raw = _enter_world(args)
    try:
        stream = Stream(sock, raw)
        # Let the login burst settle before the double-click.
        stream.wait_for(lambda _packet: False, 0, timeout=1.5)
        start = len(stream.packets)
        sock.sendall(bytes([0x06]) + ITEM_UID.to_bytes(4, "big"))
        index = stream.wait_for(lambda packet: packet[0] == 0xB0, start)
        first = _check_gump(
            "first dialog",
            None if index is None else stream.packets[index],
            FIRST_CONTROLS,
            FIRST_TEXTS,
            failures,
            passed,
            log,
        )
        if first is None:
            return

        # The client sends the reply for the button the dialog was built with.
        start = index + 1
        sock.sendall(make_gump_reply(first.serial, first.context, FIRST_BUTTON))
        expected = f"{MARKER} button|{FIRST_BUTTON}|{PANEL_Y}"
        marker = stream.wait_for(
            lambda packet: (_system_message(packet) or "").startswith(MARKER), start
        )
        report = None if marker is None else _system_message(stream.packets[marker])
        if report != expected:
            failures.append(f"button handler reported {report!r}, expected {expected!r}")
        else:
            passed.append("button handler")
        index = stream.wait_for(lambda packet: packet[0] == 0xB0, start)
        second = _check_gump(
            "second dialog",
            None if index is None else stream.packets[index],
            NEXT_CONTROLS,
            NEXT_TEXTS,
            failures,
            passed,
            log,
        )
        if second is None:
            return

        start = index + 1
        sock.sendall(make_gump_reply(second.serial, second.context, NEXT_BUTTON))
        marker = stream.wait_for(
            lambda packet: _system_message(packet) == f"{MARKER} oversize_after", start
        )
        gumps = [
            packet for packet in stream.packets[start:marker] if packet[0] == 0xB0
        ] if marker is not None else []
        if marker is None:
            failures.append("oversize dialog: the button handler did not continue")
        elif gumps:
            failures.append(f"oversize dialog: {len(gumps)} 0xB0 packet(s) sent")
        else:
            passed.append("oversize dialog refused")

        # A walk request after the replies must still be answered.
        start = len(stream.packets)
        sock.sendall(struct.pack(">BBBI", 0x02, 0x02, 0, 0))
        ack = stream.wait_for(lambda packet: packet[0] in (0x21, 0x22), start)
        if ack is None:
            failures.append("walk request after the dialog replies was not answered")
        elif stream.packets[ack][0] != 0x22 or stream.packets[ack][1] != 0:
            failures.append(f"walk request answered with {stream.packets[ack].hex()}")
        else:
            passed.append("walk acknowledged")
    except ValueError as error:
        failures.append(f"server stream is malformed: {error}")
    finally:
        sock.close()


def report_failures(fixture: Path, log_contents: str, passed: list[str]) -> list[str]:
    failures: list[str] = []
    if OVERSIZE_LOG not in log_contents:
        failures.append(f"server log does not contain {OVERSIZE_LOG!r}")
        if "oversize dialog refused" in passed:
            passed.remove("oversize dialog refused")
    report_path = fixture / "logs" / "unknown-keywords.json"
    try:
        report = json.loads(report_path.read_text(encoding="utf-8"))
        entries = report["entries"]
    except (OSError, ValueError, KeyError, TypeError) as error:
        failures.append(f"unknown-keyword report is unreadable: {error}")
        entries = None
    if entries is not None:
        keys = {(entry.get("kind"), entry.get("keyword")) for entry in entries}
        unresolved = sorted(
            f"{kind} {keyword}" for kind, keyword in keys if keyword in ("ARGO", "ARGO.*")
        )
        missing = ("function", MISSING_FUNCTION.upper())
        if unresolved:
            failures.append(f"keyword report has unresolved ARGO entries: {unresolved!r}")
        elif missing not in keys:
            failures.append(f"keyword report does not name {missing!r}: {sorted(keys)!r}")
        else:
            passed.append("keyword report")
    if RECURSION_LOG not in log_contents:
        failures.append("the self-calling layout function did not reach the recursion guard")
    else:
        passed.append("recursion guard")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2886)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    args.fixture = args.fixture.resolve()
    args.binary = args.binary.resolve()
    if not (args.fixture / "sphere.ini").is_file():
        parser.error(f"fixture configuration does not exist: {args.fixture / 'sphere.ini'}")
    if not args.binary.is_file():
        parser.error(f"server binary does not exist: {args.binary}")

    from test_world_save_roundtrip import run_server  # pylint: disable=import-outside-toplevel

    failures: list[str] = []
    passed: list[str] = []
    log: list[str] = []

    def action() -> None:
        try:
            exercise(args, failures, passed, log)
        except (OSError, RuntimeError) as error:
            failures.append(str(error))

    returncode, runner_error, log_contents = run_server(
        fixture=args.fixture,
        binary=args.binary,
        host=args.host,
        port=args.port,
        startup_timeout=args.startup_timeout,
        log_path=args.fixture / "server.log",
        action=action,
    )
    if runner_error:
        failures.append(runner_error)
    failures.extend(report_failures(args.fixture, log_contents, passed))
    failures.extend(shutdown_failures(returncode, log_contents))

    for line in log:
        print(line)
    if failures:
        print(
            f"dialog-root-dispatch probe failed: {len(passed)}/{len(CHECKS)} checks passed",
            file=sys.stderr,
        )
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print(f"dialog-root-dispatch probe passed: {len(passed)}/{len(CHECKS)} checks")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
