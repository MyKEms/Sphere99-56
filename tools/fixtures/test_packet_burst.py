#!/usr/bin/env python3
"""Measure and check client packet dispatch under bursts and floods.

A modern client sends many background requests (single-click names, status
requests, pings) while the player walks.  The server must answer them and
acknowledge walk steps promptly, keep every byte of a backlog, wait for the
rest of a packet split across TCP reads, and still limit a client that
floods it.

The script starts the server on a generated ``packet-burst`` fixture.  With
``--external`` it instead measures an already running server, for example a
reference server, and ``--measure`` reports the timings without asserting.
"""

from __future__ import annotations

import argparse
import json
import socket
import struct
import subprocess
import sys
import threading
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from run_suite import shutdown_failures, stop_server, tail, wait_for_port  # noqa: E402
from uo_huffman import COMPRESS_TABLE  # noqa: E402
from uo_packets import UO_PACKET_LENGTHS, split_packet_stream  # noqa: E402
from uo_test_client import (  # noqa: E402
    decode_game_response,
    find_start_packet,
    game_relogin,
    make_char_play,
    recv_until_game_start,
)

ACCOUNT = "PacketBurstProbe"
PASSWORD = "burst-pw"
ITEM_COUNT = 10

# The server's flood limit (CLIENT_DISPATCH_BURST and CLIENT_DISPATCH_REFILL
# in SphereSvr/spheresvr.h): a burst allowance plus a steady refill rate for
# packets other than walking, skill locks and login.
FLOOD_BURST = 100
FLOOD_RATE = 50  # packets per second

WALK_ACK_LIMIT = 0.25  # seconds; a server tick is 0.1 s
ANSWER_LIMIT = 1.0  # seconds for a whole normal burst to be answered
SANITIZER_MARKERS = ("AddressSanitizer", "UndefinedBehaviorSanitizer", "runtime error:")
RECEIVE_FAILURE_MARKERS = ("SocketsReceive threw", "Bad Msg")


def _build_trie() -> list[list[int]]:
    nodes = [[-1, -1, -1]]
    for value, entry in enumerate(COMPRESS_TABLE):
        bits, code = entry & 0xF, entry >> 4
        node = 0
        for shift in range(bits - 1, -1, -1):
            bit = (code >> shift) & 1
            if nodes[node][bit] < 0:
                nodes.append([-1, -1, -1])
                nodes[node][bit] = len(nodes) - 1
            node = nodes[node][bit]
        nodes[node][2] = value
    return nodes


TRIE = _build_trie()


def _decode_frame(raw: bytearray, start: int) -> tuple[bytes, int] | None:
    """Decode one byte-aligned Huffman frame, or return None if incomplete."""
    out = bytearray()
    node = 0
    for index in range(start, len(raw)):
        byte = raw[index]
        for shift in range(7, -1, -1):
            node = TRIE[node][(byte >> shift) & 1]
            if node < 0:
                raise ValueError(f"invalid Huffman code at byte {index}")
            value = TRIE[node][2]
            if value >= 0:
                if value == 256:
                    return bytes(out), index + 1
                out.append(value)
                node = 0
    return None


class Recorder:
    """Read a game connection and timestamp every decoded server packet."""

    def __init__(self, sock: socket.socket) -> None:
        self.sock = sock
        self.raw = bytearray()
        self.raw_pos = 0
        self.decoded = bytearray()
        self.decoded_pos = 0
        self.packets: list[tuple[float, int, bytes]] = []
        self.error: str | None = None
        self.closed = False
        self.lock = threading.Lock()
        self._stop = threading.Event()
        self.thread = threading.Thread(target=self._run, daemon=True)
        self.thread.start()

    def _run(self) -> None:
        self.sock.settimeout(0.02)
        while not self._stop.is_set():
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            except OSError as error:
                self.error = f"receive failed: {error}"
                break
            if not chunk:
                self.closed = True
                break
            now = time.monotonic()
            try:
                self._consume(chunk, now)
            except ValueError as error:
                self.error = str(error)
                break

    def _consume(self, chunk: bytes, now: float) -> None:
        self.raw.extend(chunk)
        while True:
            frame = _decode_frame(self.raw, self.raw_pos)
            if frame is None:
                break
            data, self.raw_pos = frame
            self.decoded.extend(data)
        found = []
        while self.decoded_pos < len(self.decoded):
            command = self.decoded[self.decoded_pos]
            length = UO_PACKET_LENGTHS.get(command)
            if length is None:
                raise ValueError(f"unknown server packet 0x{command:02x}")
            if length < 0:
                if len(self.decoded) - self.decoded_pos < 3:
                    break
                length = int.from_bytes(self.decoded[self.decoded_pos + 1:self.decoded_pos + 3], "big")
            if len(self.decoded) - self.decoded_pos < length:
                break
            packet = bytes(self.decoded[self.decoded_pos:self.decoded_pos + length])
            found.append((now, command, packet))
            self.decoded_pos += length
        with self.lock:
            self.packets.extend(found)

    def snapshot(self) -> list[tuple[float, int, bytes]]:
        with self.lock:
            return list(self.packets)

    def wait(self, predicate, timeout: float) -> list[tuple[float, int, bytes]]:
        deadline = time.monotonic() + timeout
        while True:
            packets = self.snapshot()
            if predicate(packets) or time.monotonic() >= deadline or self.closed or self.error:
                return packets
            time.sleep(0.01)

    def close(self) -> None:
        self._stop.set()
        self.thread.join(timeout=2.0)
        self.sock.close()


def _click(uid: int) -> bytes:
    return struct.pack(">BI", 0x09, uid)


def _status(uid: int) -> bytes:
    return struct.pack(">BIBI", 0x34, 0xEDEDEDED, 4, uid)


def _ping(sequence: int) -> bytes:
    return struct.pack(">BB", 0x73, sequence & 0xFF)


def _walk(direction: int, sequence: int) -> bytes:
    return struct.pack(">BBBI", 0x02, direction, sequence & 0xFF, 0)


def _uid(packet: bytes) -> int:
    return struct.unpack_from(">I", packet, 3)[0] if len(packet) >= 7 else 0


class Session:
    def __init__(self, host: str, port: int, game_port: int) -> None:
        sock = None
        initial = b""
        for _attempt in range(10):
            sock, _, initial = game_relogin(host, port, ACCOUNT, PASSWORD, game_port=game_port)
            if sock is not None:
                break
            time.sleep(1.0)
        if sock is None:
            raise RuntimeError("probe account did not reach the character list")
        # game_relogin returns the decoded stream.
        if find_start_packet(initial) is None:
            sock.sendall(make_char_play(0))
            initial += decode_game_response(recv_until_game_start(sock, timeout=15.0))
        start = find_start_packet(initial)
        if start is None:
            sock.close()
            raise RuntimeError("probe character did not enter the world")
        self.self_uid = struct.unpack_from(">I", start[1], 1)[0]
        login_packets = [
            (0.0, packet.command, packet.data)
            for packet in split_packet_stream(initial[start[0]:], allow_truncated=True)
        ]
        self.recorder = Recorder(sock)
        # Let the login burst settle, and keep it: it lists the visible items.
        settled = login_packets + self.recorder.wait(lambda _packets: False, 1.5)
        self.item_uids = sorted(
            {_uid(packet) & 0x7FFFFFFF for _time, command, packet in settled if command == 0x1A}
        )
        self.walk_sequence = 0

    def send(self, data: bytes) -> float:
        stamp = time.monotonic()
        self.recorder.sock.sendall(data)
        return stamp

    def walk(self, direction: int) -> tuple[int, float]:
        sequence = self.walk_sequence
        self.walk_sequence = 1 if sequence == 255 else sequence + 1
        return sequence, self.send(_walk(direction, sequence))

    def close(self) -> None:
        self.recorder.close()


def _answers(packets, since: float, command: int, uids) -> list[float]:
    return [stamp for stamp, cmd, packet in packets if stamp >= since and cmd == command and _uid(packet) in uids]


def _walk_results(packets, walks) -> list[dict]:
    results = []
    for sequence, sent in walks:
        reply = next(
            (
                (stamp, cmd)
                for stamp, cmd, packet in packets
                if stamp >= sent and cmd in (0x21, 0x22) and len(packet) > 1 and packet[1] == sequence
            ),
            None,
        )
        results.append(
            {
                "sequence": sequence,
                "latency": None if reply is None else round(reply[0] - sent, 4),
                "reply": None if reply is None else f"0x{reply[1]:02x}",
            }
        )
    return results


def _rounded(values: list[float]) -> list[float]:
    return [round(value, 4) for value in values]


def scenario_modern(args) -> dict:
    """A modern-client burst: names, status and pings, then running steps."""
    session = Session(args.host, args.port, args.game_port)
    try:
        if len(session.item_uids) < ITEM_COUNT:
            raise RuntimeError(f"expected {ITEM_COUNT} visible items, saw {len(session.item_uids)}")
        clicks = [session.item_uids[index % ITEM_COUNT] for index in range(30)]
        burst = b"".join(_click(uid) for uid in clicks)
        burst += b"".join(_status(session.self_uid) for _ in range(5))
        burst += b"".join(_ping(index) for index in range(3))
        started = session.send(burst)
        walks = []
        for _ in range(6):
            walks.append(session.walk(0x82))  # run east
            time.sleep(0.25)
        item_set = set(session.item_uids)
        packets = session.recorder.wait(
            lambda found: len(_answers(found, started, 0x1C, item_set)) >= 30
            and len(_answers(found, started, 0x11, {session.self_uid})) >= 5
            and all(r["latency"] is not None for r in _walk_results(found, walks)),
            args.deadline,
        )
        return {
            "clicks_sent": 30,
            "status_sent": 5,
            "click_answers": _rounded([t - started for t in _answers(packets, started, 0x1C, item_set)]),
            "status_answers": _rounded([t - started for t in _answers(packets, started, 0x11, {session.self_uid})]),
            "walks": _walk_results(packets, walks),
            "connection_closed": session.recorder.closed,
            "client_error": session.recorder.error,
        }
    finally:
        session.close()


def scenario_classic(args) -> dict:
    """A 3.0.x-like profile: steady steps with an occasional click."""
    session = Session(args.host, args.port, args.game_port)
    try:
        item = session.item_uids[0]
        started = session.send(_status(session.self_uid))
        walks = []
        clicks = []
        for index in range(12):
            if index % 4 == 0:
                clicks.append(session.send(_click(item)))
            walks.append(session.walk(0x86))  # run west, back past the start
            time.sleep(0.25)
        packets = session.recorder.wait(
            lambda found: len(_answers(found, started, 0x1C, {item})) >= len(clicks)
            and all(r["latency"] is not None for r in _walk_results(found, walks)),
            args.deadline,
        )
        answers = _answers(packets, started, 0x1C, {item})
        return {
            "clicks_sent": len(clicks),
            # Each answer measured from its own click.
            "click_answers": _rounded([answer - sent for answer, sent in zip(answers, clicks)]),
            "walks": _walk_results(packets, walks),
            "connection_closed": session.recorder.closed,
            "client_error": session.recorder.error,
        }
    finally:
        session.close()


def scenario_append(args) -> dict:
    """Receive more bytes while the previous read is only partly dispatched."""
    session = Session(args.host, args.port, args.game_port)
    try:
        item_set = set(session.item_uids[:3])
        first, second, third = session.item_uids[:3]
        started = session.send(_click(first) + _click(second))
        time.sleep(0.15)
        session.send(_click(third))
        packets = session.recorder.wait(lambda found: len(_answers(found, started, 0x1C, item_set)) >= 3, 3.0)
        return {
            "clicks_sent": 3,
            "click_answers": _rounded([t - started for t in _answers(packets, started, 0x1C, item_set)]),
            "connection_closed": session.recorder.closed,
            "client_error": session.recorder.error,
        }
    finally:
        session.close()


def scenario_split(args) -> dict:
    """A packet split across two TCP reads must wait for its remaining bytes."""
    session = Session(args.host, args.port, args.game_port)
    try:
        first, second = session.item_uids[:2]
        item_set = {first, second}
        packet = _click(first)
        started = session.send(packet[:2])
        time.sleep(0.3)
        session.send(packet[2:] + _click(second))
        packets = session.recorder.wait(lambda found: len(_answers(found, started, 0x1C, item_set)) >= 2, 3.0)
        return {
            "clicks_sent": 2,
            "click_answers": _rounded([t - started for t in _answers(packets, started, 0x1C, item_set)]),
            "connection_closed": session.recorder.closed,
            "client_error": session.recorder.error,
        }
    finally:
        session.close()


def scenario_flood(args) -> dict:
    """About 1,000 single-clicks per second: rate limited, yet every one answered."""
    session = Session(args.host, args.port, args.game_port)
    try:
        uids = session.item_uids
        item_set = set(uids)
        total = args.flood_packets
        started = time.monotonic()
        for offset in range(0, total, 20):
            session.send(b"".join(_click(uids[(offset + i) % len(uids)]) for i in range(min(20, total - offset))))
            time.sleep(0.02)
        sent_for = time.monotonic() - started
        # Count the answers one second after the flood started.
        time.sleep(max(0.0, started + 1.0 - time.monotonic()))
        answered_1s = len(_answers(session.recorder.snapshot(), started, 0x1C, item_set))
        expected_drain = max(0.0, (total - FLOOD_BURST) / FLOOD_RATE)
        packets = session.recorder.wait(
            lambda found: len(_answers(found, started, 0x1C, item_set)) >= total,
            args.deadline if args.measure else expected_drain + 5.0,
        )
        answers = [t - started for t in _answers(packets, started, 0x1C, item_set)]
        return {
            "clicks_sent": total,
            "send_seconds": round(sent_for, 3),
            "answered_after_1s": answered_1s,
            "answered_total": len(answers),
            "last_answer": round(answers[-1], 3) if answers else None,
            "connection_closed": session.recorder.closed,
            "client_error": session.recorder.error,
        }
    finally:
        session.close()


SCENARIOS = {
    "modern": scenario_modern,
    "classic": scenario_classic,
    "append": scenario_append,
    "split": scenario_split,
    "flood": scenario_flood,
}


def check(name: str, result: dict) -> list[str]:
    failures = []
    if result.get("client_error"):
        failures.append(f"{name}: {result['client_error']}")
    if result.get("connection_closed"):
        failures.append(f"{name}: the server closed the connection")
    answers = result.get("click_answers")
    if name in ("modern", "classic", "append", "split"):
        if len(answers) != result["clicks_sent"]:
            failures.append(f"{name}: {len(answers)} of {result['clicks_sent']} single-clicks answered")
        elif name in ("modern", "classic") and answers and max(answers) > ANSWER_LIMIT:
            failures.append(f"{name}: a single-click was answered after {max(answers):.3f} s")
    if name == "modern":
        status = result["status_answers"]
        if len(status) != result["status_sent"]:
            failures.append(f"modern: {len(status)} of {result['status_sent']} status requests answered")
        elif max(status) > ANSWER_LIMIT:
            failures.append(f"modern: a status request was answered after {max(status):.3f} s")
    for walk in result.get("walks", ()):
        if walk["latency"] is None or walk["latency"] > WALK_ACK_LIMIT or walk["reply"] != "0x22":
            failures.append(f"{name}: walk {walk['sequence']} reply {walk['reply']} after {walk['latency']} s")
    if name == "flood":
        limit = FLOOD_BURST + int(FLOOD_RATE * 1.3)
        if result["answered_after_1s"] > limit:
            failures.append(f"flood: {result['answered_after_1s']} answers in the first second, limit {limit}")
        if result["answered_after_1s"] < min(FLOOD_BURST, result["clicks_sent"]):
            failures.append(f"flood: only {result['answered_after_1s']} answers in the first second")
        if result["answered_total"] != result["clicks_sent"]:
            failures.append(f"flood: {result['answered_total']} of {result['clicks_sent']} clicks answered")
    return failures


def run_all(args) -> tuple[dict, list[str]]:
    results = {}
    failures = []
    for name in args.scenarios:
        try:
            results[name] = SCENARIOS[name](args)
        except (OSError, RuntimeError, ValueError, struct.error) as error:
            results[name] = {"error": str(error)}
            failures.append(f"{name}: {error}")
            continue
        failures.extend(check(name, results[name]))
        time.sleep(0.5)
    return results, failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path, nargs="?")
    parser.add_argument("--binary", type=Path)
    parser.add_argument("--port", type=int, default=2884)
    parser.add_argument("--game-port", type=int, help="default: port + 1000 for a started server")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--external", action="store_true", help="measure a server that is already running")
    parser.add_argument("--measure", action="store_true", help="report timings without asserting")
    parser.add_argument("--report", type=Path, help="write the timings as JSON")
    parser.add_argument("--deadline", type=float, default=6.0, help="seconds to wait for a scenario's answers")
    parser.add_argument("--flood-packets", type=int, default=400)
    parser.add_argument("--scenario", dest="scenarios", action="append", choices=tuple(SCENARIOS))
    args = parser.parse_args()
    args.scenarios = args.scenarios or list(SCENARIOS)
    if args.game_port is None:
        args.game_port = args.port if args.external else args.port + 1000

    process = None
    log_path = None
    returncode = None
    failures: list[str] = []
    results: dict = {}
    try:
        if not args.external:
            if args.fixture is None or args.binary is None:
                parser.error("a fixture and --binary are required unless --external is given")
            fixture = args.fixture.resolve()
            log_path = fixture / "server.log"
            with log_path.open("wb") as log_file:
                process = subprocess.Popen(
                    [str(args.binary.resolve()), f"-P{args.port}"],
                    cwd=fixture,
                    stdin=subprocess.DEVNULL,
                    stdout=log_file,
                    stderr=subprocess.STDOUT,
                )
            wait_for_port(args.host, args.port, 120.0)
        results, failures = run_all(args)
    except (OSError, RuntimeError, subprocess.SubprocessError) as error:
        failures.append(str(error))
    finally:
        if process is not None:
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    if log_path is not None:
        log_contents = log_path.read_text(encoding="utf-8", errors="replace")
        failures.extend(shutdown_failures(returncode, log_contents))
        receive_failures = [
            line for line in log_contents.splitlines() if any(marker in line for marker in RECEIVE_FAILURE_MARKERS)
        ]
        results["receive_failures"] = receive_failures[:20]
        failures.extend(f"server log: {line}" for line in receive_failures[:20])
        failures.extend(
            f"server log contains sanitizer output: {line}"
            for line in log_contents.splitlines()
            if any(marker in line for marker in SANITIZER_MARKERS)
        )

    report = json.dumps(results, indent=2)
    if args.report:
        args.report.write_text(report + "\n", encoding="utf-8")
    if args.measure:
        print(report)
        return 0
    if failures:
        print("packet-burst probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print(report, file=sys.stderr)
        if log_path is not None:
            print("\n--- server log tail ---", file=sys.stderr)
            print(tail(log_path), file=sys.stderr)
        return 1
    print("packet-burst probe passed: bursts answered promptly, walks acknowledged, flood limited")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
