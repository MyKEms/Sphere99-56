#!/usr/bin/env python3
"""Walk in a sector whose map blocks are read on several logical planes.

The fixture keeps a player, NPCs and items on map plane 0 and on logical
planes that read map 0 geometry, all in one world sector.  A driver item
reads every map block of that sector on every plane each second and steps
each NPC east and back.  The player walks east, waits across a periodic
sector pass that drops the whole map block cache, and walks back west.
Every player step and NPC step must be accepted, and the map lookups must
not raise server exceptions.
"""

from __future__ import annotations

import argparse
import socket
import struct
import sys
import time
from pathlib import Path

from modes.map_plane_cache import (
    ACCOUNT,
    MARKER,
    NPCS,
    PASSWORD,
    PLAYER_PLANE,
    PLAYER_POINT,
)
from run_suite import shutdown_failures


DIR_EAST = 2
DIR_WEST = 6
WALK_STEPS = 6
PASSES_PER_PHASE = 2
# A periodic sector pass runs every 128 sector pulses (32 seconds) after the
# server starts; with MAPCACHETIME=0 it drops every cached block.
CACHE_DROP_WAIT = 36.0
ERROR_MARKERS = (
    "assertion",
    "Exception in Sector",
    "Bad Msg",
)


class Session:
    """Collect the compressed game stream and decode it on demand."""

    def __init__(self, sock: socket.socket, initial: bytes) -> None:
        self.sock = sock
        self.data = bytearray(initial)
        self._decoded_size = -1
        self._packets: list = []

    def packets(self) -> list:
        from uo_packets import split_packet_stream
        from uo_test_client import decode_game_response

        if self._decoded_size != len(self.data):
            self._packets = split_packet_stream(
                decode_game_response(bytes(self.data)), allow_truncated=True
            )
            self._decoded_size = len(self.data)
        return self._packets

    def pump_until(self, predicate, timeout: float) -> bool:
        deadline = time.monotonic() + timeout
        self.sock.settimeout(0.1)
        last_check = 0.0
        while True:
            now = time.monotonic()
            if now - last_check >= 0.2 or now >= deadline:
                last_check = now
                if predicate(self.packets()):
                    return True
                if now >= deadline:
                    return False
            try:
                chunk = self.sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                return predicate(self.packets())
            self.data.extend(chunk)

    def system_messages(self) -> list[str]:
        messages = []
        for packet in self.packets():
            if packet.command != 0x1C or len(packet.data) < 45:
                continue
            text = packet.data[44:].split(b"\0", 1)[0]
            messages.append(text.decode("latin1", errors="replace"))
        return messages


def pass_count(packets) -> int:
    end = f"{MARKER}_TICK".encode("ascii")
    return sum(
        1
        for packet in packets
        if packet.command == 0x1C and packet.data[44:].split(b"\0", 1)[0] == end
    )


def walk_replies(packets) -> list[tuple[str, int]]:
    replies = []
    for packet in packets:
        if packet.command == 0x22 and len(packet.data) >= 2:
            replies.append(("ack", packet.data[1]))
        elif packet.command == 0x21 and len(packet.data) >= 2:
            replies.append(("reject", packet.data[1]))
    return replies


def parse_point(text: str) -> tuple[int, ...]:
    return tuple(int(value) for value in text.split(","))


def plane_point(point: tuple[int, int, int], plane: int, dx: int = 0) -> tuple[int, ...]:
    moved = (point[0] + dx, point[1], point[2])
    return moved + ((plane,) if plane else ())


def check_rows(messages: list[str]) -> tuple[list[str], dict[int, int], list[tuple[int, ...]]]:
    """Validate the driver rows; return failures, NPC step pairs, player points."""

    failures: list[str] = []
    steps = {plane: 0 for _, plane, _ in NPCS}
    players: list[tuple[int, ...]] = []
    for message in messages:
        if not message.startswith(MARKER + " "):
            continue
        kind, _, rest = message[len(MARKER) + 1 :].partition("|")
        if kind == "player":
            players.append(parse_point(rest))
            continue
        if kind != "npc":
            continue
        fields = rest.split("|")
        if len(fields) != 4:
            failures.append(f"malformed NPC row: {message!r}")
            continue
        plane = int(fields[0])
        before, after, back = (parse_point(field) for field in fields[1:])
        # The NPC may wander between passes; within one pass it must take
        # exactly one step east and one step back on its own plane.
        east = (before[0] + 1,) + before[1:]
        on_plane = before[3:] == ((plane,) if plane else ())
        if plane not in steps or not on_plane or after != east or back != before:
            failures.append(
                f"NPC on plane {plane} did not step east and back: "
                f"before={before} after={after} back={back}"
            )
            continue
        steps[plane] += 1
    return failures, steps, players


def walk(session: Session, direction: int, first_sequence: int, result: dict) -> None:
    """Turn toward *direction*, then take WALK_STEPS steps that way."""

    replies_before = len(walk_replies(session.packets()))
    for index in range(WALK_STEPS + 1):
        sequence = first_sequence + index
        session.sock.sendall(struct.pack(">BBBI", 0x02, direction, sequence, 0))
        session.pump_until(
            lambda packets, count=replies_before + index + 1: (
                len(walk_replies(packets)) >= count
            ),
            3.0,
        )
        # Stay below the server's walk-speed limit for unmounted players.
        time.sleep(0.35)
    result.setdefault("walk_replies", []).extend(
        walk_replies(session.packets())[replies_before:]
    )


def run_probe(host: str, port: int, pass_timeout: float, result: dict) -> None:
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_play,
        recv_until_game_start,
    )

    started = time.monotonic()
    sock, _ = game_connect(host, port, ACCOUNT, PASSWORD, game_port=port + 1000)
    if sock is None:
        raise RuntimeError("map plane probe did not reach the character list")
    try:
        sock.sendall(make_char_play(0))
        initial = recv_until_game_start(sock, timeout=30.0)
        if find_start_packet(decode_game_response(initial)) is None:
            raise RuntimeError("map plane probe character did not enter the world")
        session = Session(sock, initial)
        result["session"] = session

        def wait_passes(name: str) -> None:
            target = pass_count(session.packets()) + PASSES_PER_PHASE
            result[name] = session.pump_until(
                lambda packets: pass_count(packets) >= target, pass_timeout
            )

        # The driver reads every block on every plane before the player moves.
        wait_passes("passes_before_walk")
        walk(session, DIR_EAST, 1, result)
        wait_passes("passes_after_walk")

        # Keep the sector busy across the periodic pass that frees the cache,
        # then walk back over blocks that had to be read again.
        remaining = CACHE_DROP_WAIT - (time.monotonic() - started)
        if remaining > 0:
            session.pump_until(lambda packets: False, remaining)
        wait_passes("passes_after_cache_drop")
        walk(session, DIR_WEST, WALK_STEPS + 2, result)
        wait_passes("passes_after_return")
    finally:
        sock.close()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=2888)
    parser.add_argument("--startup-timeout", type=float, default=120.0)
    parser.add_argument("--pass-timeout", type=float, default=20.0)
    args = parser.parse_args()

    fixture = args.fixture.resolve()
    binary = args.binary.resolve()
    if not fixture.is_dir():
        parser.error(f"fixture directory does not exist: {fixture}")
    if not binary.is_file():
        parser.error(f"server binary does not exist: {binary}")

    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
    from test_world_save_roundtrip import run_server

    result: dict = {}
    failures: list[str] = []

    def exercise() -> None:
        run_probe(args.host, args.port, args.pass_timeout, result)

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

    session = result.get("session")
    messages = session.system_messages() if session is not None else []
    for phase in (
        "passes_before_walk",
        "passes_after_walk",
        "passes_after_cache_drop",
        "passes_after_return",
    ):
        if not result.get(phase):
            failures.append(f"driver did not finish {PASSES_PER_PHASE} map passes ({phase})")

    replies = result.get("walk_replies", [])
    expected_replies = [("ack", sequence) for sequence in range(1, 2 * WALK_STEPS + 3)]
    if replies != expected_replies:
        failures.append(
            f"player walk on plane {PLAYER_PLANE} was not acknowledged step by step: "
            f"replies={replies!r}"
        )

    row_failures, steps, players = check_rows(messages)
    failures.extend(row_failures)
    for plane, count in steps.items():
        if count < 4 * PASSES_PER_PHASE:
            failures.append(f"NPC on plane {plane} completed only {count} step pair(s)")
    start = plane_point(PLAYER_POINT, PLAYER_PLANE)
    east = plane_point(PLAYER_POINT, PLAYER_PLANE, WALK_STEPS)
    if east not in players:
        failures.append(f"server never reported the player at {east!r}: {players[-3:]!r}")
    if not players or players[-1] != start:
        failures.append(
            f"server-side player position was {players[-1:]!r}; expected {start!r}"
        )

    error_lines = [
        line
        for line in log_contents.splitlines()
        if any(marker in line for marker in ERROR_MARKERS)
    ]
    if error_lines:
        failures.append(f"server log contains {len(error_lines)} map lookup error line(s)")
        failures.extend(f"  {line}" for line in error_lines[:10])

    checks = 10 + len(steps)
    if failures:
        print("map plane cache probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-60:]), file=sys.stderr)
        return 1
    print(
        f"map plane cache probe passed: {checks}/{checks} checks; "
        f"{2 * WALK_STEPS} player steps on plane {PLAYER_PLANE} across a cache drop; "
        "NPC step pairs "
        + ", ".join(f"plane {plane}={count}" for plane, count in steps.items())
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
