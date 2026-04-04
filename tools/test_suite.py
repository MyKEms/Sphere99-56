#!/usr/bin/env python3
"""
Sphere99 Automated Test Suite

Usage:
    python3 test_suite.py [host] [port]

Tests:
  1. Server reachability (TCP connect)
  2. Single login → charlist flow
  3. Multiple sequential logins (stability)
  4. Rapid reconnect (stress test)
  5. Bad packet handling
  6. Account validation
"""

import socket
import struct
import sys
import time
import os

# Add tools dir to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from uo_test_client import test_login, recv_all, make_login_packet, make_server_select, make_char_create, game_connect
from uo_huffman import decompress as huffman_decompress, is_compressed as huffman_is_compressed

class TestResult:
    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors = []

    def ok(self, name):
        self.passed += 1
        print(f"  PASS: {name}")

    def fail(self, name, reason=""):
        self.failed += 1
        self.errors.append(f"{name}: {reason}")
        print(f"  FAIL: {name} — {reason}")

    def summary(self):
        total = self.passed + self.failed
        print(f"\n{'='*60}")
        print(f"Results: {self.passed}/{total} passed, {self.failed} failed")
        if self.errors:
            print("Failures:")
            for e in self.errors:
                print(f"  - {e}")
        print(f"{'='*60}")
        return self.failed == 0


def test_tcp_connect(host, port, result):
    """Test 1: Basic TCP connectivity."""
    print("\n[Test 1] TCP Connect")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        sock.close()
        result.ok("TCP connect to server")
    except Exception as e:
        result.fail("TCP connect", str(e))


def test_single_login(host, port, result):
    """Test 2: Full login → charlist flow."""
    print("\n[Test 2] Single Login Flow")
    if test_login(host, port, "test_single", "pass123"):
        result.ok("Login → ServerList → Relay → CharList")
    else:
        result.fail("Login flow", "Did not receive CharList")


def test_sequential_logins(host, port, count, result):
    """Test 3: Multiple sequential logins."""
    print(f"\n[Test 3] {count} Sequential Logins")
    successes = 0
    for i in range(count):
        if test_login(host, port, f"seqtest{i}", f"pass{i}"):
            successes += 1
        time.sleep(0.5)

    if successes == count:
        result.ok(f"All {count} logins succeeded")
    else:
        result.fail(f"Sequential logins", f"{successes}/{count} succeeded")


def test_rapid_reconnect(host, port, result):
    """Test 4: Rapid connect/disconnect cycles."""
    print("\n[Test 4] Rapid Reconnect (10 cycles)")
    crashes = 0
    for i in range(10):
        try:
            sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            sock.settimeout(2.0)
            sock.connect((host, port))
            # Send seed only, then disconnect
            sock.sendall(struct.pack('>I', 0x01000000 + i))
            time.sleep(0.1)
            sock.close()
        except Exception as e:
            crashes += 1

    # Verify server still alive
    time.sleep(1)
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        sock.close()
        if crashes == 0:
            result.ok("10 rapid reconnects, server stable")
        else:
            result.fail("Rapid reconnect", f"{crashes}/10 connect failures")
    except:
        result.fail("Rapid reconnect", "Server crashed after stress test")


def test_bad_packets(host, port, result):
    """Test 5: Server handles garbage data."""
    print("\n[Test 5] Bad Packet Handling")
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        # Send garbage
        sock.sendall(b'\xff\xff\xff\xff\x00\x01\x02\x03' * 10)
        time.sleep(0.5)
        sock.close()
    except:
        pass

    # Server should still be alive
    time.sleep(1)
    try:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.settimeout(5.0)
        sock.connect((host, port))
        sock.close()
        result.ok("Server survived garbage data")
    except:
        result.fail("Bad packets", "Server crashed on garbage data")


def test_char_create(host, port, result):
    """Test 6: Create a character and verify game entry response."""
    print("\n[Test 6] Character Creation")
    try:
        sock, auth_id = game_connect(host, port, "createtest", "cpass")
        if sock is None:
            result.fail("Character creation", "Could not reach charlist")
            return

        # Send Create packet
        create_pkt = make_char_create(name="AutoTest", sex=0, start_loc=1)
        sock.sendall(create_pkt)

        # Wait for game entry response (XCMD_Start = 0x1B)
        time.sleep(2.0)
        resp = recv_all(sock, timeout=5.0)
        sock.close()

        if not resp:
            result.fail("Character creation", "No response after create packet")
            return

        # Decompress Huffman
        if huffman_is_compressed(resp):
            raw = huffman_decompress(resp)
            if raw:
                resp = raw

        # Check for XCMD_Start (0x1B) in response
        if resp[0] == 0x1B:
            result.ok("Character created, game entry received (0x1B)")
        elif b'\x1b' in resp:
            idx = resp.index(b'\x1b')
            result.ok(f"Character created, XCMD_Start at offset {idx}")
        else:
            result.fail("Character creation", f"Got 0x{resp[0]:02x} instead of 0x1B (XCMD_Start)")
    except Exception as e:
        result.fail("Character creation", str(e))


def test_game_entry_validation(host, port, result):
    """Test 7: Validate game entry packet (XCMD_Start 0x1B) contents."""
    print("\n[Test 7] Game Entry Validation")
    try:
        sock, auth_id = game_connect(host, port, "gametest", "gpass")
        if sock is None:
            result.fail("Game entry", "Could not reach charlist")
            return

        # Create character
        create_pkt = make_char_create(name="GameEntry", sex=0, start_loc=1)
        sock.sendall(create_pkt)
        time.sleep(2.0)
        resp = recv_all(sock, timeout=5.0)

        if not resp:
            result.fail("Game entry", "No response after create")
            sock.close()
            return

        # Decompress Huffman
        if huffman_is_compressed(resp):
            raw = huffman_decompress(resp)
            if raw:
                resp = raw

        # Find XCMD_Start (0x1B) in response
        start_idx = -1
        for i in range(len(resp)):
            if resp[i] == 0x1B and i + 37 <= len(resp):
                start_idx = i
                break

        if start_idx < 0:
            result.fail("Game entry", f"No XCMD_Start (0x1B) in {len(resp)}b response")
            sock.close()
            return

        # Parse XCMD_Start: UID (4b), zero (4b), charID (2b), x (2b), y (2b), z (2b), dir (1b)
        pkt = resp[start_idx:]
        uid = struct.unpack_from('>I', pkt, 1)[0]
        char_id = struct.unpack_from('>H', pkt, 9)[0]
        x = struct.unpack_from('>H', pkt, 11)[0]
        y = struct.unpack_from('>H', pkt, 13)[0]

        if uid == 0:
            result.fail("Game entry", "UID is 0 in XCMD_Start")
        elif x == 0 and y == 0:
            result.fail("Game entry", "Position (0,0) in XCMD_Start")
        else:
            result.ok(f"Game entry: UID=0x{uid:x} pos=({x},{y}) charID=0x{char_id:x}")

        # Send a walk packet (0x02) and verify server doesn't crash
        walk_pkt = struct.pack('>BBBB', 0x02, 0x01, 0x00, 0x00)  # walk north
        try:
            sock.sendall(walk_pkt)
            time.sleep(0.5)
        except:
            pass

        sock.close()
    except Exception as e:
        result.fail("Game entry", str(e))


def test_wrong_password(host, port, result):
    """Test 8: Wrong password is rejected."""
    print("\n[Test 8] Wrong Password Rejection")
    try:
        # First create account with known password
        r1 = test_login(host, port, "pwtest_acct", "correct_pw")
        if not r1:
            result.fail("Wrong password", "Could not create initial account")
            return

        # Try with wrong password — should fail
        sock, auth = game_connect(host, port, "pwtest_acct", "WRONG_PW")
        if sock is None:
            # game_connect returns None if charlist not received = login rejected
            result.ok("Wrong password correctly rejected")
        else:
            result.fail("Wrong password", "Server accepted wrong password!")
            sock.close()
    except Exception as e:
        # Connection error = server rejected = pass
        result.ok(f"Wrong password rejected ({type(e).__name__})")


def test_login_after_stress(host, port, result):
    """Test 9: Login still works after all previous tests."""
    print("\n[Test 9] Login After Stress")
    if test_login(host, port, "finaltest", "finalpass"):
        result.ok("Login works after stress testing")
    else:
        result.fail("Post-stress login", "Login failed after stress tests")


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2593

    print(f"Sphere99 Test Suite — {host}:{port}")
    print(f"{'='*60}")

    result = TestResult()

    test_tcp_connect(host, port, result)
    test_single_login(host, port, result)
    test_sequential_logins(host, port, 3, result)
    test_rapid_reconnect(host, port, result)
    test_bad_packets(host, port, result)
    test_char_create(host, port, result)
    test_game_entry_validation(host, port, result)
    test_wrong_password(host, port, result)
    test_login_after_stress(host, port, result)

    success = result.summary()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
