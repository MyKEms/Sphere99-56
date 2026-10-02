#!/usr/bin/env python3
"""Exercise deferred party destruction across two clients in one tick.

The master is created after the member, so the server processes the master's
disband packet first (clients are newest-first).  The member then sends a
party message immediately after the disband.  A safe implementation keeps the
party object alive until the tick boundary, clears both member references, and
returns the normal no-party response instead of dereferencing freed memory.
"""

from __future__ import annotations

import argparse
import socket
import subprocess
import sys
import threading
import time
from pathlib import Path

from run_suite import SANITIZER_OUTPUT_RE, shutdown_failures, stop_server, wait_for_port


MEMBER_ACCOUNT = "PartyMember"
MEMBER_PASSWORD = "party-member-pw"
MASTER_ACCOUNT = "PartyMaster"
MASTER_PASSWORD = "party-master-pw"
PARTY_EXTDATA = 0x06
PARTYMSG_Add = 1
PARTYMSG_Msg = 4
PARTYMSG_Disband = 5
PARTYMSG_NotoInvited = 7
PARTYMSG_Accept = 8
# The server's per-client flood limit grants a burst of 100 packets and then
# 5 per server tick.  Twice the burst keeps both packets behind the refills
# even on a slow runner; the wait lets an idle client's burst refill fully.
FLOOD_PADDING = 200
FLOOD_REFILL_WAIT = 2.5


def make_ping() -> bytes:
    return b"\x73\x00"


def send_pair(packets: tuple[tuple[socket.socket, bytes], tuple[socket.socket, bytes]]) -> None:
    """Write both client queues before the next server poll can split them."""

    barrier = threading.Barrier(len(packets))
    errors: list[Exception] = []

    def send_one(sock: socket.socket, packet: bytes) -> None:
        try:
            barrier.wait()
            sock.sendall(packet)
        except Exception as error:  # propagate socket and barrier failures
            errors.append(error)

    threads = [threading.Thread(target=send_one, args=packet) for packet in packets]
    for thread in threads:
        thread.start()
    for thread in threads:
        thread.join()
    if errors:
        raise OSError(f"party packet pair send failed: {errors[0]}")


def make_extdata(code: int, payload: bytes = b"") -> bytes:
    body = bytes((code,)) + payload
    length = 5 + len(body)
    return b"\xbf" + length.to_bytes(2, "big") + PARTY_EXTDATA.to_bytes(2, "big") + body


def make_target(context: int, uid: int) -> bytes:
    packet = bytearray(19)
    packet[0] = 0x6C
    packet[1] = 0
    packet[2:6] = context.to_bytes(4, "big")
    packet[6] = 0
    packet[7:11] = uid.to_bytes(4, "big")
    return bytes(packet)


def find_party_message(data: bytes, code: int) -> bool:
    from uo_test_client import decode_game_response

    raw = decode_game_response(data)
    return find_party_code(raw, code) is not None


def find_party_code(raw: bytes, code: int) -> int | None:
    for offset in range(len(raw)):
        if raw[offset] != 0xBF or offset + 6 > len(raw):
            continue
        length = int.from_bytes(raw[offset + 1:offset + 3], "big")
        packet_type = int.from_bytes(raw[offset + 3:offset + 5], "big")
        if length >= 6 and offset + length <= len(raw) and packet_type == PARTY_EXTDATA:
            if raw[offset + 5] == code:
                return offset
    return None


def find_target_offset(raw: bytes) -> int | None:
    for offset in range(len(raw) - 18):
        if raw[offset] != 0x6C or raw[offset + 1] != 0:
            continue
        context = int.from_bytes(raw[offset + 2:offset + 6], "big")
        if context:
            return offset
    return None


def recv_until_command(sock: socket.socket, command: int, timeout: float) -> bytes:
    """Receive a compressed stream without rejecting legacy 0x68 packets."""

    from uo_test_client import decode_game_response

    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.5)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
        raw = decode_game_response(bytes(data))
        if command == 0x6C and find_target_offset(raw) is not None:
            return bytes(data)
        if command == 0xBF and find_party_code(raw, PARTYMSG_NotoInvited) is not None:
            return bytes(data)
    return bytes(data)


def recv_until_party_code(sock: socket.socket, code: int, timeout: float) -> bytes:
    """Receive until the requested party response arrives."""

    from uo_test_client import decode_game_response

    data = bytearray()
    deadline = time.monotonic() + timeout
    sock.settimeout(0.5)
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(65536)
        except socket.timeout:
            continue
        if not chunk:
            break
        data.extend(chunk)
        raw = decode_game_response(bytes(data))
        if find_party_code(raw, code) is not None:
            return bytes(data)
    return bytes(data)


def run_probe(fixture: Path, binary: Path, port: int, startup_timeout: float) -> int:
    tools_path = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(tools_path))
    from uo_test_client import (
        decode_game_response,
        find_start_packet,
        game_connect,
        make_char_create,
        recv_until_game_start,
    )

    log_path = fixture / "server.log"
    # The fixture's bounded destruction marker is an INFO-level event.  Keep
    # it visible in the disposable runtime without enabling network tracing.
    ini_path = fixture / "sphere.ini"
    ini_path.write_text(
        ini_path.read_text(encoding="ascii").replace("DEBUGLEVEL=0", "DEBUGLEVEL=1"),
        encoding="ascii",
    )
    failures: list[str] = []
    returncode: int | None = None
    member: socket.socket | None = None
    master: socket.socket | None = None
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=fixture,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port("127.0.0.1", port, startup_timeout)

            # Create the member first, then the master.  CServer inserts new
            # clients at the head, making the packet order deterministic.
            member, _ = game_connect(
                "127.0.0.1", port, MEMBER_ACCOUNT, MEMBER_PASSWORD, game_port=port + 1000
            )
            if member is None:
                failures.append("member did not reach the character list")
            else:
                member.sendall(make_char_create(name="PartyMember", start_loc=1))
                response = recv_until_game_start(member, timeout=30.0)
                member_start = find_start_packet(decode_game_response(response))
                if member_start is None:
                    failures.append("member character did not enter the world")
                    member_uid = 0
                else:
                    member_uid = int.from_bytes(member_start[1][1:5], "big")

            master, _ = game_connect(
                "127.0.0.1", port, MASTER_ACCOUNT, MASTER_PASSWORD, game_port=port + 1000
            )
            if master is None:
                failures.append("master did not reach the character list")
            else:
                master.sendall(make_char_create(name="PartyMaster", start_loc=1))
                response = recv_until_game_start(master, timeout=30.0)
                master_start = find_start_packet(decode_game_response(response))
                if master_start is None:
                    failures.append("master character did not enter the world")
                    master_uid = 0
                else:
                    master_uid = int.from_bytes(master_start[1][1:5], "big")

            if member is not None and master is not None and member_uid and master_uid:
                # Ask the master to invite the member, then answer the target
                # cursor with the member serial.
                master.sendall(make_extdata(PARTYMSG_Add))
                target_response = recv_until_command(master, 0x6C, timeout=10.0)
                target_raw = decode_game_response(target_response)
                target_offset = find_target_offset(target_raw)
                if target_offset is None:
                    failures.append("master did not receive the party target cursor")
                else:
                    context = int.from_bytes(target_raw[target_offset + 2:target_offset + 6], "big")
                    master.sendall(make_target(context, member_uid))
                    invite = recv_until_party_code(member, PARTYMSG_NotoInvited, timeout=10.0)
                    if not find_party_message(invite, 7):
                        failures.append(
                            "member did not receive the party invitation: "
                            f"target={target_raw[target_offset:target_offset + 19].hex()} "
                            f"response={decode_game_response(invite).hex()[:256]} "
                            f"member_uid={member_uid:#x} master_uid={master_uid:#x}"
                        )
                    else:
                        member.sendall(make_extdata(PARTYMSG_Accept, master_uid.to_bytes(4, "big")))
                        # Wait for the party manifest to settle before forcing
                        # the same-tick disband/member-message ordering.
                        master_manifest = recv_until_party_code(master, PARTYMSG_Add, timeout=10.0)
                        member_manifest = recv_until_party_code(member, PARTYMSG_Add, timeout=10.0)
                        if not find_party_message(master_manifest, PARTYMSG_Add):
                            failures.append(
                                "master did not receive the party manifest: "
                                f"{decode_game_response(master_manifest).hex()[:256]}"
                            )
                        if not find_party_message(member_manifest, PARTYMSG_Add):
                            failures.append(
                                "member did not receive the party manifest: "
                                f"{decode_game_response(member_manifest).hex()[:256]}"
                            )

                        # The master is processed first.  Keep the member's
                        # packet inside the deferred-destruction window: the
                        # same number of pings ahead of each packet uses up
                        # both clients' flood-limit burst, so the refills,
                        # which come at the same server ticks for both,
                        # release the two packets in the same tick.
                        time.sleep(FLOOD_REFILL_WAIT)
                        padding = make_ping() * FLOOD_PADDING
                        send_pair(
                            (
                                (master, padding + make_extdata(PARTYMSG_Disband)),
                                (
                                    member,
                                    padding
                                    + make_extdata(
                                        PARTYMSG_Msg,
                                        "party object survived".encode("utf-16-be") + b"\0\0",
                                    ),
                                ),
                            )
                        )
                        response = recv_until_party_code(member, PARTYMSG_Msg, timeout=10.0)
                        if not find_party_message(response, PARTYMSG_Msg):
                            failures.append("member did not receive the no-party response")

            time.sleep(0.2)
            if process.poll() is not None:
                failures.append(f"server exited before probe completion with status {process.returncode}")
        except (OSError, RuntimeError, ValueError) as error:
            failures.append(str(error))
        finally:
            for sock in (master, member):
                if sock is not None:
                    try:
                        sock.close()
                    except OSError:
                        pass
            try:
                returncode = stop_server(process)
            except (OSError, RuntimeError, subprocess.TimeoutExpired) as error:
                failures.append(f"server shutdown failed: {error}")

    log_contents = log_path.read_text(encoding="utf-8", errors="replace")
    failures.extend(shutdown_failures(returncode, log_contents))
    if SANITIZER_OUTPUT_RE.search(log_contents):
        failures.append("server log contains sanitizer output")
    destruction_count = log_contents.count("CPartyDef destroyed")
    if destruction_count != 1:
        failures.append(
            f"party destruction diagnostic occurred {destruction_count} times; expected exactly once"
        )
    response_count = log_contents.count("CPartyDef no-party response")
    if response_count != 1:
        failures.append(
            f"no-party response diagnostic occurred {response_count} times; expected exactly once"
        )
    destruction_offset = log_contents.find("CPartyDef destroyed")
    response_offset = log_contents.find("CPartyDef no-party response")
    if (
        destruction_offset >= 0
        and response_offset >= 0
        and destruction_offset < response_offset
    ):
        failures.append("party was destroyed before the callback response completed")

    if failures:
        print("party lifetime probe failed:", file=sys.stderr)
        for failure in failures:
            print(f"- {failure}", file=sys.stderr)
        print("\n--- server log (tail) ---", file=sys.stderr)
        print("\n".join(log_contents.splitlines()[-80:]), file=sys.stderr)
        return 1

    print("party lifetime probe passed: deferred disband and member response were observed")
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, default=2864)
    parser.add_argument("--startup-timeout", type=float, default=90.0)
    args = parser.parse_args()
    return run_probe(args.fixture.resolve(), args.binary.resolve(), args.port, args.startup_timeout)


if __name__ == "__main__":
    raise SystemExit(main())
