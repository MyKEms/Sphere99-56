#!/usr/bin/env python3
"""Test UO login flow against Sphere99 server with Huffman decompression."""
import socket, struct, time, sys

# UO Huffman decompression tree (standard)
HUFFMAN_TABLE = [
    # Each entry: (left_child, right_child) for internal nodes
    # or (0, value) for leaf nodes
    # This is the standard UO Huffman table from the protocol documentation
]

def uo_decompress(data):
    """Decompress UO Huffman-encoded data. Simplified version."""
    # UO uses a specific Huffman tree. For now, try raw pass-through
    # since the server might send uncompressed for NoCrypt clients.
    return data

def test_login(host='127.0.0.1', port=2593, account='gm_wednesday'):
    print(f"=== UO Login Test: {account}@{host}:{port} ===\n")

    # Step 1: Login (XCMD_ServersReq = 0x80)
    print("Step 1: Sending login request...")
    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s.settimeout(5)
    s.connect((host, port))

    # 4-byte seed
    s.send(struct.pack('>I', 0x7F000001))
    time.sleep(0.1)

    # Login packet (62 bytes)
    pkt = bytearray(62)
    pkt[0] = 0x80  # XCMD_ServersReq
    acct_bytes = account.encode('ascii') + b'\x00'
    pkt[1:1+len(acct_bytes)] = acct_bytes
    pkt[30] = 0  # null terminator
    pkt[35] = 0  # empty password
    pkt[60] = 0
    pkt[61] = 0xFF
    s.send(bytes(pkt))
    time.sleep(0.3)

    resp = s.recv(4096)
    if not resp:
        print("  ERROR: No response")
        return

    if resp[0] == 0xA8:
        pktlen = struct.unpack('>H', resp[1:3])[0]
        vercode = resp[3]
        nservers = struct.unpack('>H', resp[4:6])[0]
        print(f"  Server List (0xA8): {nservers} server(s), vercode=0x{vercode:02x}")
        # Parse server entries (40 bytes each: 2 index + 32 name + 1 full + 1 tz + 4 ip)
        off = 6
        for i in range(nservers):
            if off + 40 > len(resp): break
            idx = struct.unpack('>H', resp[off:off+2])[0]
            name = resp[off+2:off+34].split(b'\x00')[0].decode('ascii', 'replace')
            print(f"    Server {idx}: \"{name}\"")
            off += 40
    elif resp[0] == 0x82:
        print(f"  Login denied: code={resp[1]}")
        s.close()
        return
    else:
        print(f"  Unknown response: 0x{resp[0]:02x}")
        s.close()
        return

    # Step 2: Select server 0
    print("\nStep 2: Selecting server 0...")
    sel = bytearray(3)
    sel[0] = 0xA0
    sel[1] = 0
    sel[2] = 0
    s.send(bytes(sel))
    time.sleep(1)
    s.settimeout(2)
    try:
        resp = s.recv(4096)
    except socket.timeout:
        print("  Timeout on relay - server may need flush trigger")
        # Send dummy byte to trigger flush
        try:
            s.send(b'\x00')
            time.sleep(0.5)
            resp = s.recv(4096)
        except:
            print("  Still no relay response")
            s.close()
            return

    if resp and resp[0] == 0x8C:
        ip = f"{resp[1]}.{resp[2]}.{resp[3]}.{resp[4]}"
        port2 = struct.unpack('>H', resp[5:7])[0]
        acct_key = struct.unpack('>I', resp[7:11])[0]
        print(f"  Relay (0x8C): {ip}:{port2}, key=0x{acct_key:08x}")
    else:
        print(f"  Unexpected: 0x{resp[0]:02x}" if resp else "  No response")
        s.close()
        return

    s.close()

    # Step 3: Connect to game server with CharListReq
    print(f"\nStep 3: Connecting to game server {ip}:{port2}...")
    time.sleep(0.5)
    s2 = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    s2.settimeout(5)
    s2.connect((ip, port2))

    # Seed
    s2.send(struct.pack('>I', 0x7F000001))
    time.sleep(0.1)

    # CharListReq (65 bytes)
    pkt2 = bytearray(65)
    pkt2[0] = 0x91  # XCMD_CharListReq
    struct.pack_into('>I', pkt2, 1, acct_key)
    acct_bytes = account.encode('ascii') + b'\x00'
    pkt2[5:5+len(acct_bytes)] = acct_bytes
    pkt2[34] = 0
    pkt2[35] = 0  # empty password
    pkt2[64] = 0
    s2.send(bytes(pkt2))
    time.sleep(1)

    # Read all response data
    data = b''
    while True:
        try:
            chunk = s2.recv(4096)
            if not chunk: break
            data += chunk
        except socket.timeout:
            break

    print(f"  Response: {len(data)} bytes")
    if data:
        print(f"  First 40 bytes: {data[:40].hex()}")

        # Check if it's compressed (game mode) or raw
        if data[0] == 0xA9:
            # Uncompressed CharList!
            parse_charlist(data, 0)
        elif data[0] == 0x82:
            codes = {0:'Invalid', 1:'InUse', 2:'Blocked', 3:'BadPassword', 4:'Other'}
            print(f"  Login error: {codes.get(data[1], data[1])}")
        else:
            print(f"  Response is likely Huffman-compressed (first byte: 0x{data[0]:02x})")
            print(f"  This is expected for game mode connections.")
            print(f"  A real UO client would decompress this automatically.")

            # Try to find any ASCII strings in the response
            strings = []
            i = 0
            while i < len(data):
                if 32 <= data[i] <= 126:
                    start = i
                    while i < len(data) and 32 <= data[i] <= 126:
                        i += 1
                    if i - start >= 3:
                        strings.append(data[start:i].decode('ascii'))
                i += 1
            if strings:
                print(f"  Embedded strings: {strings[:10]}")

    s2.close()
    print("\n=== Test Complete ===")

def parse_charlist(data, offset):
    """Parse XCMD_CharList (0xA9) packet."""
    pktlen = struct.unpack('>H', data[offset+1:offset+3])[0]
    nchars = data[offset+4]
    print(f"  CharList (0xA9): len={pktlen}, chars={nchars}")
    off = offset + 9
    for j in range(min(nchars, 7)):
        name = data[off:off+30].split(b'\x00')[0].decode('ascii', 'replace')
        print(f"    [{j}] \"{name}\"")
        off += 60

if __name__ == '__main__':
    account = sys.argv[1] if len(sys.argv) > 1 else 'gm_wednesday'
    host = sys.argv[2] if len(sys.argv) > 2 else '127.0.0.1'
    port = int(sys.argv[3]) if len(sys.argv) > 3 else 2593
    test_login(host, port, account)
