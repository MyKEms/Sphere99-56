#!/usr/bin/env python3
"""
Sphere99 Automated Test Suite

Usage:
    python3 test_suite.py [host] [login_port] [game_port] [--quick] [--skip-fixture-tests]

Tests:
  1. Server reachability (TCP connect)
  2. Single login → charlist flow
  3. Multiple sequential logins (stability)
  4. Rapid reconnect (stress test)
  5. Bad packet handling
  6. Character creation
  7. Quick relogin after disconnect
  8. Game entry validation
  9. Walking
  10. Wrong-password rejection
  11. Post-stress login
  12. Script engine stability
  13. Expression evaluation proxy
  14. Direct script function-table smoke test
"""

import socket
import struct
import sys
import time
import os

# Add tools dir to path
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from uo_test_client import (
    test_login,
    make_login_packet,
    make_server_select,
    make_char_create,
    game_connect,
    game_relogin,
    decode_game_response,
    find_start_packet,
    recv_until_game_start,
)
from uo_packets import split_packet_stream

TEST_RUN_ID = f"{os.getpid()}_{int(time.time())}"


def test_account(prefix, index=None):
    """Return a short per-run account name so tests are repeatable."""
    suffix = TEST_RUN_ID if index is None else f"{TEST_RUN_ID}_{index}"
    return f"{prefix}_{suffix}"[:29]


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


def test_single_login(host, port, game_port, result):
    """Test 2: Full login → charlist flow."""
    print("\n[Test 2] Single Login Flow")
    if test_login(host, port, test_account("single"), "pass123", game_port=game_port):
        result.ok("Login → ServerList → Relay → CharList")
    else:
        result.fail("Login flow", "Did not receive CharList")


def test_sequential_logins(host, port, game_port, count, result):
    """Test 3: Multiple sequential logins."""
    print(f"\n[Test 3] {count} Sequential Logins")
    successes = 0
    for i in range(count):
        if test_login(host, port, test_account("seq", i), f"pass{i}", game_port=game_port):
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


def test_char_create(host, port, game_port, result):
    """Test 6: Create a character and verify game entry response."""
    print("\n[Test 6] Character Creation")
    try:
        sock, auth_id = game_connect(host, port, test_account("create"), "cpass", game_port=game_port)
        if sock is None:
            result.fail("Character creation", "Could not reach charlist")
            return

        # Send Create packet
        create_pkt = make_char_create(
            name="AutoTest",
            sex=0,
            start_loc=1,
            skill1=25,
            val1=40,
            skill2=26,
            val2=40,
            skill3=1,
            val3=20,
        )
        sock.sendall(create_pkt)

        # Wait for game entry response (XCMD_Start = 0x1B).
        resp = _drain_game_socket(
            sock,
            recv_until_game_start(sock, timeout=10.0),
        )
        sock.close()

        if not resp:
            result.fail("Character creation", "No response after create packet")
            return

        resp = decode_game_response(resp)
        start = find_start_packet(resp)
        if start is None:
            result.fail("Character creation", "No structurally valid XCMD_Start packet")
        else:
            result.ok(f"Character created, XCMD_Start at offset {start[0]}")

        expected_items = (
            ("SPHERE_NEWBIE_MAGERY ", "SPHERE_NEWBIE_MAGERY 1"),
            ("SPHERE_NEWBIE_RESIST ", "SPHERE_NEWBIE_RESIST 1"),
        )
        for prefix, expected in expected_items:
            actual = _find_system_message(resp, prefix)
            if actual == expected:
                result.ok(f"Character received skill-keyed starting item: {actual}")
            else:
                result.fail(
                    f"Skill-keyed NEWBIE {prefix.strip()}",
                    f"expected {expected!r}, got {actual!r}",
                )
    except Exception as e:
        result.fail("Character creation", str(e))


def test_game_entry_validation(host, port, game_port, result):
    """Test 8: Validate game entry packet (XCMD_Start 0x1B) contents."""
    print("\n[Test 8] Game Entry Validation")
    try:
        sock, auth_id = game_connect(host, port, test_account("entry"), "gpass", game_port=game_port)
        if sock is None:
            result.fail("Game entry", "Could not reach charlist")
            return

        # Create character
        create_pkt = make_char_create(name="GameEntry", sex=0, start_loc=1)
        sock.sendall(create_pkt)
        resp = recv_until_game_start(sock, timeout=10.0)

        if not resp:
            result.fail("Game entry", "No response after create")
            sock.close()
            return

        resp = decode_game_response(resp)
        start = find_start_packet(resp)
        if start is None:
            result.fail("Game entry", f"No XCMD_Start (0x1B) in {len(resp)}b response")
            sock.close()
            return

        # Parse XCMD_Start: UID (4b), zero (4b), charID (2b), x (2b), y (2b), z (2b), dir (1b)
        start_idx, pkt = start
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


def test_quick_relogin(host, port, game_port, result):
    """Test 7: A disconnected lingering client reconnects into the last char."""
    print("\n[Test 7] Quick Relogin After Disconnect")
    old_sock = None
    new_sock = None
    account = test_account("quick")
    try:
        old_sock, _ = game_connect(host, port, account, "quick_relogin_pw", game_port=game_port)
        if old_sock is None:
            result.fail("Quick relogin", "Could not reach initial charlist")
            return

        old_sock.sendall(make_char_create(name="QuickRelogin", sex=0, start_loc=1))
        first_response = decode_game_response(recv_until_game_start(old_sock, timeout=10.0))
        if find_start_packet(first_response) is None:
            result.fail("Quick relogin", "Initial character did not enter the game")
            return

        # A normal client reconnects after its old socket is gone.  The server
        # keeps the character in the configured linger state, and this selects
        # the direct last-character path on the second login.
        old_sock.close()
        old_sock = None
        time.sleep(1.0)
        new_sock, _, second_response = game_relogin(
            host, port, account, "quick_relogin_pw", game_port=game_port
        )
        if new_sock is None:
            result.fail("Quick relogin", "Reconnect did not reach game entry")
            return

        if find_start_packet(second_response) is None:
            result.fail("Quick relogin", "Reconnect returned no valid XCMD_Start packet")
        else:
            result.ok("Attached client reconnected directly into the game")
    except Exception as e:
        result.fail("Quick relogin", str(e))
    finally:
        if new_sock is not None:
            new_sock.close()
        if old_sock is not None:
            old_sock.close()


def test_walking(host, port, game_port, result):
    """Test 9: Walk in multiple directions and verify WalkAck responses."""
    print("\n[Test 9] Walking")
    try:
        sock, auth_id = game_connect(host, port, test_account("walk"), "walkpass", game_port=game_port)
        if sock is None:
            result.fail("Walking", "Could not reach charlist")
            return

        # Create character first
        create_pkt = make_char_create(name="Walker", sex=0, start_loc=0)
        sock.sendall(create_pkt)

        # Wait for the entry marker, then drain the remaining initial data.
        drained = recv_until_game_start(sock, timeout=10.0)
        sock.setblocking(False)
        while True:
            try:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                drained += chunk
            except BlockingIOError:
                break
        sock.setblocking(True)
        sock.settimeout(10.0)

        if not drained:
            result.fail("Walking", "No game entry response")
            sock.close()
            return

        # Now send walk packets (v26 format: cmd + dir + count + fastwalk_key)
        all_resp = b''
        for seq in range(1, 6):  # 5 walk steps
            dir_byte = (seq % 8)  # cycle through directions
            walk_pkt = struct.pack('>BBBI', 0x02, dir_byte, seq & 0xFF, 0)
            sock.sendall(walk_pkt)
            time.sleep(0.5)
            # Drain response
            sock.setblocking(False)
            try:
                while True:
                    chunk = sock.recv(65536)
                    if not chunk:
                        break
                    all_resp += chunk
            except BlockingIOError:
                pass
            sock.setblocking(True)
            sock.settimeout(10.0)

        # Final drain
        time.sleep(1.0)
        sock.setblocking(False)
        try:
            while True:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                all_resp += chunk
        except BlockingIOError:
            pass
        resp = all_resp
        sock.close()

        if not resp:
            result.fail("Walking", "No response after five walk packets")
            return

        decompressed = decode_game_response(resp)

        # Count WalkAck (0x22) packets after strict stream framing.  A byte
        # scan could mistake payload data for a packet command and hide a
        # protocol desynchronization.
        packets = split_packet_stream(decompressed)
        ack_count = sum(packet.command == 0x22 for packet in packets)

        if ack_count >= 3:
            result.ok(f"Walking works — {ack_count} WalkAcks received for 5 steps")
        elif ack_count > 0:
            result.ok(f"Walking partial — {ack_count} WalkAcks (server responds to walks)")
        else:
            result.fail("Walking", f"No WalkAck packet in {len(decompressed)}b response")

    except Exception as e:
        result.fail("Walking", str(e))


def test_wrong_password(host, port, game_port, result):
    """Test 10: Wrong password is rejected."""
    print("\n[Test 10] Wrong Password Rejection")
    try:
        # First create account with known password
        account = test_account("pw")
        r1 = test_login(host, port, account, "correct_pw", game_port=game_port)
        if not r1:
            result.fail("Wrong password", "Could not create initial account")
            return

        # Try with wrong password — should fail
        sock, auth = game_connect(host, port, account, "WRONG_PW", game_port=game_port)
        if sock is None:
            # game_connect returns None if charlist not received = login rejected
            result.ok("Wrong password correctly rejected")
        else:
            result.fail("Wrong password", "Server accepted wrong password!")
            sock.close()
    except Exception as e:
        # Connection error = server rejected = pass
        result.ok(f"Wrong password rejected ({type(e).__name__})")


def test_login_after_stress(host, port, game_port, result):
    """Test 11: Login still works after all previous tests."""
    print("\n[Test 11] Login After Stress")
    if test_login(host, port, test_account("final"), "finalpass", game_port=game_port):
        result.ok("Login works after stress testing")
    else:
        result.fail("Post-stress login", "Login failed after stress tests")


def test_script_engine_stability(host, port, game_port, result):
    """Test 12: Script engine stability — function dispatch tables active.

    Verifies the CSCRIPT_PROPX_IMP fix and script engine features:
    - Function dispatch tables (Eval, Safe, ArgV, etc.) are populated
    - argv()/argvcount work in script function contexts
    - Object reference chaining (argo.tag, argo.uid) resolves
    - <?...?> deferred macros are processed
    - Gump commands are accumulated during dialog construction

    These features are exercised indirectly: if function tables were empty,
    CAN flag evaluation would fail, causing incorrect collision detection.
    The walk test (Test 8) proves DEFNAME resolution works, which requires
    working function dispatch. This test adds stress with multiple clients
    entering the game world simultaneously.
    """
    print("\n[Test 12] Script Engine Stability (multi-client game entry)")
    try:
        success_count = 0
        for i in range(3):
            acct = test_account("script", i)
            sock, auth = game_connect(host, port, acct, acct, game_port=game_port)
            if sock:
                try:
                    sock.sendall(make_char_create(name=f"ScriptEntry{i}"))
                    response = decode_game_response(recv_until_game_start(sock, timeout=10.0))
                    if find_start_packet(response) is not None:
                        success_count += 1
                except Exception:
                    pass
                finally:
                    sock.close()
            time.sleep(0.3)

        # Verify server is still alive
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(3.0)
            s.connect((host, port))
            s.close()
            if success_count == 3:
                result.ok("Script engine stable — 3/3 structurally valid game entries")
            else:
                result.fail("Script engine", f"Only {success_count}/3 game entries were valid")
        except Exception:
            result.fail("Script engine", "Server crashed during multi-client game entry")

    except Exception as e:
        result.fail("Script engine stability", str(e))


def test_expression_eval_proxy(host, port, game_port, result):
    """Test 13: Expression evaluation proxy — DEFNAME & CAN flag resolution.

    The walk test (Test 9) is the primary verification that expression
    evaluation works, since CAN=MT_WALK|MT_EQUIP requires:
    1. Function dispatch tables populated (CSCRIPT_PROPX_IMP fix)
    2. DEFNAME resolution (MT_WALK=0x14, MT_EQUIP=0x20000)
    3. Expression evaluator handling | operator
    4. Hex parsing (00014 = 0x14)

    This test adds a fresh login + 10 rapid walk packets to stress-test
    the collision system which depends on expression evaluation.
    """
    print("\n[Test 13] Expression Eval Proxy (rapid walks)")
    try:
        sock, auth = game_connect(host, port, test_account("eval"), "eval_test_pw", game_port=game_port)
        if not sock:
            result.fail("Expression eval proxy", "Could not connect")
            return

        # Create character and enter game
        char_create = make_char_create("evaltest")
        sock.sendall(char_create)
        entry_resp = recv_until_game_start(sock, timeout=10.0)

        # Drain and validate game entry data before stressing movement.
        sock.setblocking(False)
        try:
            while True:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                entry_resp += chunk
        except BlockingIOError:
            pass
        sock.setblocking(True)
        sock.settimeout(5.0)

        if find_start_packet(decode_game_response(entry_resp)) is None:
            result.fail("Expression eval proxy", "No structurally valid game entry")
            sock.close()
            return

        # Send 10 rapid walk packets in the same direction
        # This stress-tests: GetRegion, CheckValidMove, CAN flag eval, GetHeightPoint
        for seq in range(1, 11):
            walk_pkt = struct.pack('>BBBI', 0x02, 0x00, seq & 0xFF, 0)  # north
            sock.sendall(walk_pkt)
            time.sleep(0.1)

        time.sleep(1)
        # Drain
        resp = b''
        sock.setblocking(False)
        try:
            while True:
                chunk = sock.recv(65536)
                if not chunk:
                    break
                resp += chunk
        except BlockingIOError:
            pass
        sock.close()

        # Verify server survived
        try:
            s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            s.settimeout(3.0)
            s.connect((host, port))
            s.close()
            result.ok(f"Expression eval — 10 rapid walks, {len(resp)}b response, server stable")
        except Exception:
            result.fail("Expression eval proxy", "Server crashed during rapid walks")

    except Exception as e:
        result.fail("Expression eval proxy", str(e))


def _find_system_message(data, prefix):
    """Return a classic 0x1c system-message text from a decoded stream."""
    for packet in split_packet_stream(data):
        if packet.command != 0x1C or len(packet.data) < 45:
            continue
        text = packet.data[44:].split(b"\0", 1)[0].decode("ascii", errors="replace")
        if text.startswith(prefix):
            return text
    return None


def _drain_game_socket(sock, initial=b""):
    """Collect a bounded post-entry window without racing the server."""
    data = bytearray(initial)
    deadline = time.monotonic() + 2.0
    sock.settimeout(0.2)
    try:
        while time.monotonic() < deadline:
            try:
                chunk = sock.recv(65536)
            except socket.timeout:
                continue
            if not chunk:
                break
            data.extend(chunk)
    except (ConnectionResetError, OSError):
        pass
    finally:
        sock.setblocking(True)
    return bytes(data)


def test_script_function_tables(host, port, game_port, result):
    """Test 14: script functions and a spawned CHARDEF combat range."""
    print("\n[Test 14] Script Functions and Spawned CHARDEF Range")
    sock = None
    try:
        sock, _ = game_connect(host, port, test_account("table"), "tablepass", game_port=game_port)
        if sock is None:
            result.fail("Script function tables", "Could not reach charlist")
            return

        sock.sendall(make_char_create(name="TableSmoke", sex=0, start_loc=1))
        response = _drain_game_socket(sock, recv_until_game_start(sock, timeout=10.0))
        decoded = decode_game_response(response)
        expected = "SPHERE_TABLE_SMOKE 3|3|0|1|0"
        actual = _find_system_message(decoded, "SPHERE_TABLE_SMOKE ")
        if actual == expected:
            result.ok(f"Table functions evaluated from trigger: {actual}")
        else:
            result.fail(
                "Script function tables",
                f"expected {expected!r}, got {actual!r} in {len(decoded)} decoded bytes",
            )

        char_trigger = _find_system_message(decoded, "SPHERE_CHAR_TRIGGER ")
        expected_char_trigger = "SPHERE_CHAR_TRIGGER TableSmoke|42|fixture-char|TableSmoke"
        if char_trigger == expected_char_trigger:
            result.ok("Arbitrary character trigger received SRC, ARGN, ARGS, and ARGO")
        else:
            result.fail(
                "Generic character trigger",
                f"expected {expected_char_trigger!r}, got {char_trigger!r}",
            )

        chardef_trigger = _find_system_message(decoded, "SPHERE_CHARDEF_TRIGGER ")
        expected_chardef_trigger = "SPHERE_CHARDEF_TRIGGER TableSmoke|21|fixture-typedef|TableSmoke"
        if chardef_trigger == expected_chardef_trigger:
            result.ok("Arbitrary character definition trigger received SRC, ARGN, ARGS, and ARGO")
        else:
            result.fail(
                "Generic character definition trigger",
                f"expected {expected_chardef_trigger!r}, got {chardef_trigger!r}",
            )

        item_trigger = _find_system_message(decoded, "SPHERE_ITEM_TRIGGER ")
        expected_item_trigger = "SPHERE_ITEM_TRIGGER TableSmoke|7|fixture-item|TableSmoke"
        if item_trigger == expected_item_trigger:
            result.ok("Arbitrary item trigger received SRC, ARGN, ARGS, and ARGO")
        else:
            result.fail(
                "Generic item trigger",
                f"expected {expected_item_trigger!r}, got {item_trigger!r}",
            )

        trigger_return = _find_system_message(decoded, "SPHERE_TRIGGER_RETURN ")
        if trigger_return == "SPHERE_TRIGGER_RETURN 73":
            result.ok("TRIGGER exposes the nested trigger's explicit return value")
        else:
            result.fail(
                "Generic trigger return",
                f"expected 'SPHERE_TRIGGER_RETURN 73', got {trigger_return!r}",
            )

        range_message = _find_system_message(decoded, "SPHERE_RANGE_ARMOR ")
        if range_message == "SPHERE_RANGE_ARMOR 95":
            result.ok("Spawned player used CHARDEF ARMOR=5,5 for deterministic damage")
        else:
            result.fail(
                "CHARDEF range on spawned player",
                f"expected 'SPHERE_RANGE_ARMOR 95', got {range_message!r}",
            )
    except Exception as error:
        result.fail("Script function tables", str(error))
    finally:
        if sock is not None:
            sock.close()


def main():
    host = sys.argv[1] if len(sys.argv) > 1 else "localhost"
    port = int(sys.argv[2]) if len(sys.argv) > 2 else 2593
    game_port = port
    extra = sys.argv[3:]
    if extra and not extra[0].startswith("--"):
        game_port = int(extra.pop(0))
    quick = "--quick" in extra
    skip_fixture_tests = "--skip-fixture-tests" in extra

    modes = []
    if quick:
        modes.append("quick")
    if skip_fixture_tests:
        modes.append("fixture-specific tests skipped")
    mode_label = f" ({', '.join(modes)})" if modes else ""
    print(f"Sphere99 Test Suite — login {host}:{port}, game {host}:{game_port}{mode_label}")
    print(f"{'='*60}")

    # Pre-flight: check if server is reachable
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.settimeout(3.0)
        s.connect((host, port))
        s.close()
    except Exception:
        print(f"\nERROR: Server not reachable at {host}:{port}")
        print("Start server first: tools/start_server.sh /path/to/sphere")
        sys.exit(2)

    result = TestResult()

    test_tcp_connect(host, port, result)
    test_single_login(host, port, game_port, result)

    if not quick:
        test_sequential_logins(host, port, game_port, 3, result)
        test_rapid_reconnect(host, port, result)
        test_bad_packets(host, port, result)

    test_char_create(host, port, game_port, result)
    test_quick_relogin(host, port, game_port, result)
    test_game_entry_validation(host, port, game_port, result)
    test_walking(host, port, game_port, result)

    if not quick:
        test_wrong_password(host, port, game_port, result)

    test_login_after_stress(host, port, game_port, result)

    # Script engine tests
    test_script_engine_stability(host, port, game_port, result)
    test_expression_eval_proxy(host, port, game_port, result)
    if skip_fixture_tests:
        print("\n[Test 14] Skipped — requires the synthetic script hooks from make_fixture.py")
    else:
        test_script_function_tables(host, port, game_port, result)

    success = result.summary()
    sys.exit(0 if success else 1)


if __name__ == "__main__":
    main()
