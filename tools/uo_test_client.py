#!/usr/bin/env python3
"""
UO Test Client — simulates ClassicUO login flow for automated testing.

Usage:
    python3 uo_test_client.py [host] [port] [account] [password]

Defaults: localhost 2593 testuser testpass

Simulates the full login sequence:
  1. Connect → send seed (4 bytes)
  2. Send Login packet (0x80) → receive ServerList (0xA8)
  3. Send ServerSelect (0xA0) → receive Relay (0x8C)
  4. Reconnect to relayed IP:port
  5. Send CharListReq (0x91) → receive CharList (0xA9)
  6. If chars exist: send CharPlay (0x5D) → receive game packets

All packets are NoCrypt (encryption=0).
Game connection responses are Huffman-compressed.
"""

import socket
import struct
import sys
import time
import os

# Import Huffman module from same directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from uo_huffman import decompress as huffman_decompress, is_compressed as huffman_is_compressed

def hexdump(data, prefix=""):
    """Print hex dump of data."""
    hex_str = ' '.join(f'{b:02x}' for b in data[:64])
    if len(data) > 64:
        hex_str += f' ... ({len(data)} bytes total)'
    print(f"{prefix}{hex_str}")

def make_login_packet(account, password):
    """Build XCMD_ServersReq (0x80) — 62 bytes."""
    pkt = bytearray(62)
    pkt[0] = 0x80  # XCMD_ServersReq
    # Account name (30 bytes, null-terminated)
    acct_bytes = account.encode('ascii')[:29]
    pkt[1:1+len(acct_bytes)] = acct_bytes
    # Password (30 bytes, null-terminated)
    pass_bytes = password.encode('ascii')[:29]
    pkt[31:31+len(pass_bytes)] = pass_bytes
    # NextLoginKey byte
    pkt[61] = 0x00
    return bytes(pkt)

def make_server_select(index=0):
    """Build XCMD_ServerSelect (0xA0) — 3 bytes."""
    pkt = bytearray(3)
    pkt[0] = 0xA0  # XCMD_ServerSelect
    struct.pack_into('>H', pkt, 1, index)
    return bytes(pkt)

def make_charlist_req(account, password, auth_id=0x7f000001):
    """Build XCMD_CharListReq (0x91) — 65 bytes."""
    pkt = bytearray(65)
    pkt[0] = 0x91  # XCMD_CharListReq
    struct.pack_into('>I', pkt, 1, auth_id)  # Account ID from relay
    # Account name (30 bytes)
    acct_bytes = account.encode('ascii')[:29]
    pkt[5:5+len(acct_bytes)] = acct_bytes
    # Password (30 bytes)
    pass_bytes = password.encode('ascii')[:29]
    pkt[35:35+len(pass_bytes)] = pass_bytes
    return bytes(pkt)

def make_char_play(slot=0):
    """Build XCMD_CharPlay (0x5D) — 73 bytes."""
    pkt = bytearray(73)
    pkt[0] = 0x5D  # XCMD_CharPlay
    # Pattern (4 bytes)
    struct.pack_into('>I', pkt, 1, 0xEDEDEDED)
    # Char name (30 bytes) — ignored, server uses slot
    # Password (30 bytes) — ignored
    # Slot index
    struct.pack_into('>I', pkt, 65, slot)
    # Client IP
    struct.pack_into('>I', pkt, 69, 0x7f000001)
    return bytes(pkt)

def make_char_create(name="TestChar", sex=0, start_loc=1, str_val=30, dex_val=25, int_val=25,
                     skill1=0, val1=50, skill2=1, val2=50, skill3=17, val3=1):
    """Build XCMD_Create (0x00) — 104 bytes. Creates a new character.

    Args:
        name: character name (max 30 chars)
        sex: 0=male, 1=female
        start_loc: starting location index (1-based)
        str_val, dex_val, int_val: base stats (must sum to ≤80)
        skill1/2/3: skill indices, val1/2/3: skill values (must sum to ≤100)
    """
    pkt = bytearray(104)
    pkt[0] = 0x00  # XCMD_Create
    # Pattern (4 bytes)
    struct.pack_into('>I', pkt, 1, 0xEDEDEDED)
    # Character name (30 bytes)
    name_bytes = name.encode('ascii')[:29]
    pkt[5:5+len(name_bytes)] = name_bytes
    # Password (30 bytes) — empty
    # Sex
    pkt[70] = sex
    # Stats
    pkt[71] = str_val
    pkt[72] = dex_val
    pkt[73] = int_val
    # Skills
    pkt[74] = skill1
    pkt[75] = val1
    pkt[76] = skill2
    pkt[77] = val2
    pkt[78] = skill3
    pkt[79] = val3
    # Start location (1-based)
    pkt[80] = start_loc
    # Skin hue
    struct.pack_into('>H', pkt, 81, 0x03EA)  # default skin hue
    # Hair
    struct.pack_into('>H', pkt, 83, 0x203B)  # hair ID
    struct.pack_into('>H', pkt, 85, 0x044E)  # hair hue
    # Beard (0 = none)
    struct.pack_into('>H', pkt, 87, 0x0000)
    struct.pack_into('>H', pkt, 89, 0x0000)
    return bytes(pkt)


def game_connect(host, port, account, password):
    """Full login sequence, return (socket, auth_id) on game connection or (None, None)."""
    import time as _time

    # Phase 1: Login
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    try:
        sock.connect((host, port))
    except Exception:
        return None, None

    seed = struct.pack('>I', 0x01000001)
    sock.sendall(seed + make_login_packet(account, password))
    resp = recv_all(sock, timeout=3.0)
    if not resp or resp[0] != 0xA8:
        sock.close()
        return None, None

    # Phase 2: Server Select
    sock.sendall(make_server_select(0))
    resp = recv_all(sock, timeout=3.0)
    if not resp or resp[0] != 0x8C:
        sock.close()
        return None, None

    relay_port = struct.unpack_from('>H', resp, 5)[0]
    auth_id = struct.unpack_from('>I', resp, 7)[0]
    sock.close()
    _time.sleep(0.3)

    # Phase 3: Game connection
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(10.0)
    try:
        sock.connect((host, relay_port))
    except Exception:
        return None, None

    game_seed = struct.pack('>I', auth_id)
    sock.sendall(game_seed + make_charlist_req(account, password, auth_id))

    # Wait for charlist
    _time.sleep(1.0)
    resp = recv_all(sock, timeout=5.0)
    if not resp:
        sock.close()
        return None, None

    # Decompress Huffman if needed
    if huffman_is_compressed(resp):
        raw = huffman_decompress(resp)
        if raw:
            resp = raw

    if resp[0] != 0xA9:
        sock.close()
        return None, None

    return sock, auth_id


def recv_all(sock, timeout=10.0):
    """Receive all available data with timeout."""
    sock.settimeout(timeout)
    data = bytearray()
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data.extend(chunk)
            sock.settimeout(2.0)  # Followup timeout for additional data
    except socket.timeout:
        pass
    return bytes(data)

def parse_server_list(data):
    """Parse XCMD_ServerList (0xA8) response."""
    if len(data) < 6 or data[0] != 0xA8:
        return None
    length = struct.unpack_from('>H', data, 1)[0]
    sysinfo = data[3]
    count = struct.unpack_from('>H', data, 4)[0]
    print(f"  ServerList: {count} server(s), {length} bytes")
    servers = []
    offset = 6
    for i in range(count):
        if offset + 40 > len(data):
            break
        idx = struct.unpack_from('>H', data, offset)[0]
        name = data[offset+2:offset+34].split(b'\x00')[0].decode('ascii', errors='replace')
        full_pct = data[offset+34]
        tz = data[offset+35]
        ip_bytes = data[offset+36:offset+40]
        ip = f"{ip_bytes[3]}.{ip_bytes[2]}.{ip_bytes[1]}.{ip_bytes[0]}"
        print(f"  Server[{i}]: idx={idx} name='{name}' ip={ip}")
        servers.append((idx, name, ip))
        offset += 40
    return servers

def parse_relay(data):
    """Parse XCMD_Relay (0x8C) response."""
    if len(data) < 11 or data[0] != 0x8C:
        return None
    ip = f"{data[1]}.{data[2]}.{data[3]}.{data[4]}"
    port = struct.unpack_from('>H', data, 5)[0]
    auth = struct.unpack_from('>I', data, 7)[0]
    print(f"  Relay: ip={ip} port={port} auth=0x{auth:08x}")
    return (ip, port, auth)

def parse_char_list(data):
    """Parse XCMD_CharList (0xA9) response."""
    if len(data) < 9 or data[0] != 0xA9:
        return None
    length = struct.unpack_from('>H', data, 1)[0]
    char_count = data[3]
    print(f"  CharList: {char_count} character(s), {length} bytes")
    chars = []
    offset = 4
    for i in range(char_count):
        if offset + 60 > len(data):
            break
        name = data[offset:offset+30].split(b'\x00')[0].decode('ascii', errors='replace')
        # password field (30 bytes) — skip
        if name:
            print(f"  Char[{i}]: '{name}'")
            chars.append(name)
        else:
            print(f"  Char[{i}]: (empty slot)")
        offset += 60
    return chars

def parse_login_error(data):
    """Parse XCMD_LogBad (0x82) response."""
    if len(data) < 2 or data[0] != 0x82:
        return None
    code = data[1]
    errors = {0: "No account", 1: "Already in use", 2: "Blocked", 3: "Bad password", 4: "Other/timeout"}
    print(f"  LoginError: code={code} ({errors.get(code, 'unknown')})")
    return code

def test_login(host="localhost", port=2593, account="testuser", password="testpass"):
    """Run full login test sequence."""
    print(f"\n{'='*60}")
    print(f"UO Test Client — {host}:{port} account='{account}'")
    print(f"{'='*60}")

    # Phase 1: Connect and send seed + login
    print(f"\n[1] Connecting to {host}:{port}...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    try:
        sock.connect((host, port))
    except Exception as e:
        print(f"  FAILED: {e}")
        return False
    print(f"  Connected!")

    # Send seed (4 bytes) + login packet together
    seed = struct.pack('>I', 0x01000001)
    login_pkt = make_login_packet(account, password)
    print(f"\n[2] Sending seed + Login (0x80)...")
    hexdump(seed + login_pkt, "  TX: ")
    sock.sendall(seed + login_pkt)

    # Receive ServerList
    print(f"\n[3] Waiting for ServerList (0xA8)...")
    resp = recv_all(sock, timeout=5.0)
    if not resp:
        print("  FAILED: No response")
        sock.close()
        return False
    hexdump(resp, "  RX: ")

    # Check for login error
    if resp[0] == 0x82:
        parse_login_error(resp)
        sock.close()
        return False

    servers = parse_server_list(resp)
    if not servers:
        print(f"  FAILED: Expected 0xA8, got 0x{resp[0]:02x}")
        sock.close()
        return False

    # Phase 2: Select server
    print(f"\n[4] Sending ServerSelect (0xA0) index=0...")
    select_pkt = make_server_select(0)
    sock.sendall(select_pkt)

    # Receive Relay
    print(f"\n[5] Waiting for Relay (0x8C)...")
    resp = recv_all(sock, timeout=5.0)
    if not resp:
        print("  FAILED: No response")
        sock.close()
        return False
    hexdump(resp, "  RX: ")

    relay = parse_relay(resp)
    if not relay:
        print(f"  FAILED: Expected 0x8C, got 0x{resp[0]:02x}")
        sock.close()
        return False

    relay_ip, relay_port, auth_id = relay
    sock.close()
    time.sleep(0.2)

    # Phase 3: Reconnect to game server
    # Use localhost instead of relay IP (Docker)
    game_host = host
    print(f"\n[6] Reconnecting to {game_host}:{relay_port} (game)...")
    sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    sock.settimeout(5.0)
    try:
        sock.connect((game_host, relay_port))
    except Exception as e:
        print(f"  FAILED: {e}")
        return False
    print(f"  Connected!")

    # Send seed + CharListReq
    game_seed = struct.pack('>I', auth_id)
    charlist_pkt = make_charlist_req(account, password, auth_id)
    print(f"\n[7] Sending seed + CharListReq (0x91)...")
    hexdump(game_seed + charlist_pkt, "  TX: ")
    sock.sendall(game_seed + charlist_pkt)

    # Receive CharList
    print(f"\n[8] Waiting for CharList (0xA9)...")
    resp = recv_all(sock, timeout=5.0)
    if not resp:
        print("  FAILED: No response (server may have crashed)")
        sock.close()
        return False
    hexdump(resp, "  RX(raw): ")

    # Game connection uses Huffman compression — decompress first
    if huffman_is_compressed(resp):
        raw = huffman_decompress(resp)
        if raw:
            print(f"  Huffman: {len(resp)}b compressed → {len(raw)}b decompressed")
            resp = raw
        else:
            print(f"  Huffman decompression failed")

    if resp[0] == 0x82:
        parse_login_error(resp)
        sock.close()
        return False

    chars = parse_char_list(resp)
    if chars is None:
        print(f"  Got packet 0x{resp[0]:02x} instead of CharList")
        # Try to find 0xA9 in the decompressed response
        idx = resp.find(b'\xA9')
        if idx > 0:
            print(f"  Found 0xA9 at offset {idx}, retrying...")
            chars = parse_char_list(resp[idx:])

    if chars is not None:
        print(f"\n{'='*60}")
        print(f"SUCCESS! CharList received with {len(chars)} character(s)")
        if chars:
            print(f"Characters: {', '.join(c for c in chars if c)}")
        print(f"{'='*60}")
        sock.close()
        return True

    print(f"\n  Could not parse CharList response")
    sock.close()
    return False

if __name__ == "__main__":
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2593
    account = sys.argv[3] if len(sys.argv) > 3 else "testuser"
    password = sys.argv[4] if len(sys.argv) > 4 else "testpass"

    success = test_login(host, port, account, password)
    sys.exit(0 if success else 1)
