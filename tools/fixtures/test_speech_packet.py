#!/usr/bin/env python3
"""Strictly decode one NPC speech after a double-click side effect."""

from __future__ import annotations

import argparse
import socket
import sys
import time
from pathlib import Path
from typing import Callable, Optional

from modes.speech_packet import (
    ACCOUNT,
    PASSWORD,
    REAL_NOTICE,
    REAL_SPEECH_OPTIONS,
)
from run_suite import shutdown_failures

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from test_packet_burst import _decode_frame  # noqa: E402
from uo_packets import UO_PACKET_LENGTHS  # noqa: E402
from uo_test_client import (  # noqa: E402
    decode_game_response,
    find_start_packet,
    game_relogin,
    make_char_play,
    make_unicode_talk,
    recv_until_game_start,
)


MAX_VARIABLE_PACKET = 4096


class Stream:
    """Decode every packet and reject a stray variable-packet header."""

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
                if length < 3 or length > MAX_VARIABLE_PACKET:
                    raise ValueError(
                        f"server packet 0x{command:02x} declares implausible length {length} "
                        f"at {self.decoded_pos}: {self.decoded[max(0, self.decoded_pos - 80):self.decoded_pos + 16].hex()}"
                    )
            if len(self.decoded) - self.decoded_pos < length:
                break
            self.packets.append(
                bytes(self.decoded[self.decoded_pos:self.decoded_pos + length])
            )
            self.decoded_pos += length

    def wait_for(
        self, match: Callable[[bytes], bool], start: int, timeout: float = 8.0
    ) -> Optional[int]:
        deadline = time.monotonic() + timeout
        self.sock.settimeout(0.2)
        while True:
            for index in range(start, len(self.packets)):
                if match(self.packets[index]):
                    return index
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


def _speech_uid(packet: bytes) -> int:
    return int.from_bytes(packet[3:7], "big") if len(packet) >= 7 else 0


def _speech_text(packet: bytes) -> str:
    if len(packet) < 45:
        return ""
    return packet[44:].split(b"\0", 1)[0].decode("ascii")


def _walk(sequence: int) -> bytes:
    """Send one ordinary walk request after the helper's packet burst."""

    return bytes((0x02, 0, sequence & 0xFF, 0, 0, 0, 0))


def exercise(args: argparse.Namespace, failures: list[str], log: list[str]) -> None:
    sock, raw = _enter_world(args)
    try:
        stream = Stream(sock, raw)
        stream.wait_for(lambda _packet: False, 0, timeout=0.1)
        player_start = find_start_packet(decode_game_response(raw))
        if player_start is None:
            failures.append("player start packet was missing")
            return
        player_uid = int.from_bytes(player_start[1][1:5], "big")
        stream.wait_for(lambda _packet: False, 0, timeout=1.0)
        npc_candidates = [
            int.from_bytes(packet[3:7], "big")
            for packet in stream.packets
            if packet[0] == 0x78
            and len(packet) >= 9
            and int.from_bytes(packet[7:9], "big") == 0x0190
            and int.from_bytes(packet[3:7], "big") != player_uid
        ]
        if not npc_candidates:
            failures.append("login did not publish the synthetic NPC")
            return
        npc_uid = npc_candidates[-1]
        start = len(stream.packets)
        sock.sendall(bytes([0x06]) + npc_uid.to_bytes(4, "big"))
        speech_index = stream.wait_for(
            lambda packet: packet[0] == 0x1C and _speech_uid(packet) == npc_uid,
            start,
        )
        after = stream.packets[start:]
        log.append(f"decoded packets after double-click: {len(after)}")
        if speech_index is None:
            failures.append("NPC speech packet did not arrive")
            return
        speech = stream.packets[speech_index]
        speeches = [
            packet
            for packet in after
            if packet[0] == 0x1C and _speech_uid(packet) == npc_uid
        ]
        if len(speeches) != 1:
            failures.append(f"expected one NPC speech packet, got {len(speeches)}")
        expected_speech = tuple(
            text.replace("<SRC.NAME>", "SpeechPacketProbe")
            for text in REAL_SPEECH_OPTIONS
        )
        if _speech_text(speech) not in expected_speech:
            failures.append(
                f"tutorial speech was not expanded: {_speech_text(speech)!r}"
            )
        notifications = [
            packet
            for packet in after
            if packet[0] == 0x1C and _speech_uid(packet) == 0
        ]
        if len(notifications) != 3:
            failures.append(
                f"expected two raw and one generated notification packet, got {len(notifications)}"
            )
        else:
            if _speech_text(notifications[-1]) != REAL_NOTICE:
                failures.append(
                    f"generated notification text was wrong: {_speech_text(notifications[-1])!r}"
                )
            log.append("two raw notifications, one generated notification, and one NPC speech packet decoded")

        for packet in after:
            if packet[0] == 0x1C and int.from_bytes(packet[1:3], "big") != len(packet):
                failures.append(
                    f"speech packet length mismatch: declared {int.from_bytes(packet[1:3], 'big')} actual {len(packet)}"
                )

        # An unrelated speech must still be safe while the nearby synthetic
        # NPC is considered as a listener.  NPC_OnHearName returns a negative
        # no-match sentinel; the server must normalize it before indexing the
        # incoming text buffer and keep the connection usable.
        speech_start = len(stream.packets)
        sock.sendall(make_unicode_talk("unrelated speech"))
        stream.wait_for(lambda _packet: False, speech_start, timeout=0.5)

        walk_start = len(stream.packets)
        sock.sendall(_walk(1))
        walk_index = stream.wait_for(
            lambda packet: packet[0] == 0x22 and len(packet) >= 2 and packet[1] == 1,
            walk_start,
        )
        if walk_index is None:
            failures.append("walk after the tutorial helper packet was not acknowledged")
        else:
            log.append("walk after tutorial speech acknowledged")
    except ValueError as error:
        failures.append(f"server stream is malformed: {error}")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2941)
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
    log: list[str] = []

    def action() -> None:
        try:
            exercise(args, failures, log)
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
    failures.extend(shutdown_failures(returncode, log_contents))

    for line in log:
        print(line)
    if failures:
        print(f"speech-packet probe failed", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        return 1
    print("speech-packet probe passed")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
