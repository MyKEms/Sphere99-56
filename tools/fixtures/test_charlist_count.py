#!/usr/bin/env python3
"""Check character-list slot counts for encrypted and NoCrypt clients."""

from __future__ import annotations

import argparse
import shutil
import socket
import subprocess
import sys
import tempfile
import time
from pathlib import Path

TOOLS = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(TOOLS))

from run_suite import shutdown_failures, stop_server, wait_for_port
from test_official_crypt import GAME_LOGIN_CAPTURE
from uo_huffman import decompress, is_compressed
from uo_packets import split_packet_stream


ACCOUNT = "test_player"
PASSWORD = "test-pass"
GAME_SEED = 0x7F000001
COMPRESS_XOR_STREAM = bytes(
    (0x05, 0x92, 0x66, 0x23, 0x67, 0x14, 0xE3, 0x62,
     0xDC, 0x60, 0x8C, 0xD6, 0xFE, 0x7C, 0x25, 0x69)
)
GAME_STREAM_PREFIX = bytes.fromhex("b6a0fef9")

# The 3.0.6m game stream continues after the 0x91 setup request.  This is a
# public synthetic packet: it deletes slot zero using an empty character
# password and keeps the stream state from the request above.
DELETE_CAPTURE = bytes.fromhex(
    "36952f85cbfaba0fb65a35ca15f384df48a3dc138168c1e491ff7a42f49dcb69b1b5b6d27cdf3d"
)


def _plain_char_list_request() -> bytes:
    packet = bytearray(65)
    packet[0] = 0x91
    packet[1:5] = GAME_SEED.to_bytes(4, "big")
    packet[5:5 + len(ACCOUNT)] = ACCOUNT.encode("ascii")
    packet[35:35 + len(PASSWORD)] = PASSWORD.encode("ascii")
    return bytes(packet)


def _read_response(sock: socket.socket) -> bytes:
    data = bytearray()
    sock.settimeout(0.2)
    deadline = time.monotonic() + 2.0
    while time.monotonic() < deadline:
        try:
            chunk = sock.recv(8192)
        except socket.timeout:
            if data:
                break
            continue
        if not chunk:
            break
        data.extend(chunk)
        deadline = time.monotonic() + 0.5
    return bytes(data)


def _decode_response(data: bytes, *, encrypted: bool, stream_offset: int) -> tuple[list, int]:
    if not data:
        raise AssertionError("server returned no character-list response")
    if encrypted and stream_offset == 0:
        if data[:2] != GAME_STREAM_PREFIX[:2]:
            raise AssertionError(
                "encrypted game response is missing its stock prefix "
                f"(got={data[:4].hex()})"
            )
        # A zero-UID legacy view compresses to three bytes; a populated view
        # uses four.  The first encrypted Huffman byte tells us which framing
        # the server selected, so try both lengths and retain the parseable one.
        candidates = []
        for prefix_len in (3, 4):
            tail = data[prefix_len:]
            decoded = bytes(
                value ^ COMPRESS_XOR_STREAM[(prefix_len + index) & 0x0F]
                for index, value in enumerate(tail)
            )
            try:
                plain = decompress(decoded) if is_compressed(decoded) else decoded
                packets = split_packet_stream(plain)
            except (TypeError, ValueError):
                continue
            if packets and packets[0].command in (0xA9, 0x82):
                return packets, prefix_len + len(tail)
            candidates.append((packets, prefix_len + len(tail)))
        if candidates:
            return candidates[0]
        raise AssertionError("encrypted game response did not contain a parseable packet")
    if encrypted:
        data = bytes(
            value ^ COMPRESS_XOR_STREAM[(stream_offset + index) & 0x0F]
            for index, value in enumerate(data)
        )
        stream_offset += len(data)
    if is_compressed(data):
        data = decompress(data)
    return split_packet_stream(data), stream_offset


def _account_text(char_count: int) -> str:
    lines = [f"[{ACCOUNT}]", f"PASSWORD={PASSWORD}"]
    for serial in range(1, char_count + 1):
        lines.append(f"CHARUID={serial}")
    if char_count:
        lines.append("LASTCHARUID=1")
    lines.extend(("[EOF]", ""))
    return "\n".join(lines)


def _chars_text(char_count: int) -> str:
    lines = [
        "TITLE=Sphere encrypted character-list fixture",
        "VERSION=0.99",
        "SAVECOUNT=0",
    ]
    for serial in range(1, char_count + 1):
        lines.extend(
            (
                "[WORLDCHAR c_MAN]",
                f"SERIAL={serial}",
                f"ACCOUNT={ACCOUNT}",
                f"NAME=Fixture {serial}",
                "STR=100",
                "INT=100",
                "DEX=100",
                "HITS=100",
                "MAXHITS=100",
                "MANA=100",
                "STAM=100",
                "P=128,128,0",
            )
        )
    lines.extend(("[EOF]", ""))
    return "\n".join(lines)


def _prepare_runtime(source: Path, destination: Path, char_count: int, encrypted: bool) -> None:
    shutil.copytree(source, destination)
    ini = (destination / "sphere.ini").read_text(encoding="ascii")
    ini = "\n".join(
        line for line in ini.splitlines()
        if not line.upper().startswith(("CLIENTVERSION=", "MINCHARDELETETIME="))
    ) + "\n"
    if encrypted:
        ini = ini.replace(
            "SERVNAME=Sphere99 synthetic fixture",
            "SERVNAME=Sphere99 synthetic fixture\nCLIENTVERSION=3.0.6",
        )
    if char_count == 2:
        ini = ini.replace(
            "CLIENTVERSION=3.0.6" if encrypted else "SERVNAME=Sphere99 synthetic fixture",
            ("CLIENTVERSION=3.0.6" if encrypted else "SERVNAME=Sphere99 synthetic fixture")
            + "\nMINCHARDELETETIME=0",
            1,
        )
    (destination / "accounts" / "sphereaccu.scp").write_text(
        _account_text(char_count), encoding="ascii"
    )
    (destination / "save" / "spherechars.scp").write_text(
        _chars_text(char_count), encoding="ascii"
    )
    (destination / "sphere.ini").write_text(ini, encoding="ascii")


def _check_char_list(packets: list, command: int, expected_city: int | None = None) -> list[str]:
    matching = [packet.data for packet in packets if packet.command == command]
    if not matching:
        return [f"response did not contain 0x{command:02x}"]
    data = matching[0]
    if len(data) < 4:
        return [f"0x{command:02x} response is too short"]
    count = data[3]
    failures = []
    if count != 5:
        failures.append(f"0x{command:02x} slot count was {count}; expected 5")
    if expected_city is not None:
        city_offset = 4 + count * 60
        if city_offset >= len(data):
            failures.append("0xA9 response ended before the city count")
        elif data[city_offset] != expected_city:
            failures.append(
                "ClassicUO parser saw city count "
                f"{data[city_offset]}; expected {expected_city}"
            )
    return failures


def _run_server(binary: Path, runtime: Path, port: int, *, encrypted: bool, delete: bool) -> list[str]:
    log_path = runtime / "server.log"
    failures: list[str] = []
    returncode: int | None = None
    stream_offset = 0
    with log_path.open("wb") as log_file:
        process = subprocess.Popen(
            [str(binary), f"-P{port}"],
            cwd=runtime,
            stdin=subprocess.DEVNULL,
            stdout=log_file,
            stderr=subprocess.STDOUT,
        )
        try:
            wait_for_port("127.0.0.1", port, 90.0)
            with socket.create_connection(("127.0.0.1", port), timeout=5.0) as sock:
                if encrypted:
                    sock.sendall(GAME_LOGIN_CAPTURE)
                else:
                    sock.sendall(GAME_SEED.to_bytes(4, "big") + _plain_char_list_request())
                try:
                    packets, stream_offset = _decode_response(
                        _read_response(sock), encrypted=encrypted, stream_offset=stream_offset
                    )
                except (AssertionError, ValueError) as error:
                    failures.append(str(error))
                    packets = []
                failures.extend(_check_char_list(packets, 0xA9, expected_city=1))

                if delete:
                    sock.sendall(DELETE_CAPTURE)
                    try:
                        packets, _ = _decode_response(
                            _read_response(sock), encrypted=True, stream_offset=stream_offset
                        )
                    except (AssertionError, ValueError) as error:
                        failures.append(str(error))
                        packets = []
                    failures.extend(_check_char_list(packets, 0x86))
        except (OSError, RuntimeError) as error:
            failures.append(str(error))
        finally:
            returncode = stop_server(process)
    failures.extend(
        shutdown_failures(
            returncode,
            log_path.read_text(encoding="utf-8", errors="replace"),
        )
    )
    return failures


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("fixture", type=Path)
    parser.add_argument("--binary", type=Path, required=True)
    parser.add_argument("--port", type=int, required=True)
    args = parser.parse_args()

    failures: list[str] = []
    with tempfile.TemporaryDirectory(prefix="sphere-charlist-count-") as temporary:
        root = Path(temporary)
        cases = (
            ("encrypted zero-character 0xA9", 0, True, False),
            ("encrypted two-character 0xA9", 2, True, False),
            ("encrypted post-delete 0x86", 2, True, True),
            ("NoCrypt padded 0xA9", 0, False, False),
        )
        for name, char_count, encrypted, delete in cases:
            runtime = root / name.replace(" ", "-")
            case_failures = _run_case(
                args.fixture,
                runtime,
                args.binary.resolve(),
                args.port,
                char_count=char_count,
                encrypted=encrypted,
                delete=delete,
            )
            if case_failures:
                failures.extend(f"{name}: {failure}" for failure in case_failures)
            else:
                print(f"charlist count passed: {name}")
    if failures:
        print("encrypted character-list count fixture failed:")
        for failure in failures:
            print(f"- {failure}")
        return 1
    return 0


def _run_case(
    source: Path,
    destination: Path,
    binary: Path,
    port: int,
    *,
    char_count: int,
    encrypted: bool,
    delete: bool,
) -> list[str]:
    _prepare_runtime(source, destination, char_count, encrypted)
    return _run_server(binary, destination, port, encrypted=encrypted, delete=delete)


if __name__ == "__main__":
    raise SystemExit(main())
