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
from uo_test_client import test_login, recv_all, make_login_packet, make_server_select

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


def test_login_after_stress(host, port, result):
    """Test 6: Login still works after all previous tests."""
    print("\n[Test 6] Login After Stress")
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
    test_login_after_stress(host, port, result)

    success = result.summary()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
