#!/usr/bin/env python3
"""Offline regression checks for the public headless protocol helpers."""

import io
import struct
import unittest
import zlib
from contextlib import redirect_stdout
from unittest.mock import patch

from uo_huffman import compress as huffman_compress, decompress as huffman_decompress
from uo_test_client import (
    make_char_create,
    make_char_play,
    make_charlist_req,
    make_login_packet,
    make_server_select,
)
from uo_packets import PacketStreamError, split_packet_stream
from uo_gumps import (
    GumpPacketError,
    make_gump_reply,
    make_skill_use,
    parse_gump_dialog,
    parse_gump_reply,
    parse_skill_use,
)
from test_suite import TestResult, test_char_create


def recorded_gump_packet(command=0xB0):
    layout = (
        b"{resizepic 0 0 5054 240 120}"
        b"{button 20 20 4005 4006 1 0 7}"
        b"{text 60 20 0 0}"
    )
    text = "Choose class".encode("utf-16-be")
    text_lines = struct.pack(">H", 1) + struct.pack(">H", len(text) // 2) + text
    if command == 0xB0:
        header = struct.pack(">BHIIIIH", 0xB0, 0, 0x01020304, 0x05060708, 10, 20, len(layout) + 1)
        packet = header + layout + b"\0" + text_lines
        return packet[:1] + struct.pack(">H", len(packet)) + packet[3:]

    compressed_layout = zlib.compress(layout)
    compressed_text = zlib.compress(text_lines[2:])
    # The compressed-text header is after the text count; keep the explicit
    # count/length fields separate so the packet mirrors the wire format.
    body = (
        struct.pack(">II", 4 + len(compressed_layout), len(layout))
        + compressed_layout
        + struct.pack(">III", 1, 4 + len(compressed_text), len(text_lines[2:]))
        + compressed_text
    )
    header = struct.pack(">BHIIII", 0xDD, 0, 0x01020304, 0x05060708, 10, 20)
    packet = header + body
    return packet[:1] + struct.pack(">H", len(packet)) + packet[3:]


class ProtocolPacketTests(unittest.TestCase):
    def test_login_packet_is_fixed_width(self):
        packet = make_login_packet("account", "password")
        self.assertEqual(len(packet), 62)
        self.assertEqual(packet[0], 0x80)
        self.assertEqual(packet[1:8], b"account")
        self.assertEqual(packet[31:39], b"password")
        self.assertEqual(packet[61], 0)

    def test_server_select_is_network_order(self):
        self.assertEqual(make_server_select(0x1234), b"\xa0\x12\x34")

    def test_charlist_request_contains_relay_auth_and_credentials(self):
        packet = make_charlist_req("account", "password", auth_id=0x12345678)
        self.assertEqual(len(packet), 65)
        self.assertEqual(packet[0], 0x91)
        self.assertEqual(struct.unpack_from(">I", packet, 1)[0], 0x12345678)
        self.assertEqual(packet[5:12], b"account")
        self.assertEqual(packet[35:43], b"password")

    def test_char_play_is_fixed_width(self):
        packet = make_char_play(slot=3)
        self.assertEqual(len(packet), 73)
        self.assertEqual(packet[0], 0x5D)
        self.assertEqual(struct.unpack_from(">I", packet, 65)[0], 3)

    def test_modern_character_create_layout(self):
        packet = make_char_create(
            name="PacketProbe", sex=1, start_loc=7, str_val=30, dex_val=25, int_val=25
        )
        self.assertEqual(len(packet), 104)
        self.assertEqual(packet[0], 0x00)
        self.assertEqual(struct.unpack_from(">I", packet, 1)[0], 0xEDEDEDED)
        self.assertEqual(struct.unpack_from(">I", packet, 5)[0], 0xFFFFFFFF)
        self.assertEqual(packet[9], 0)
        self.assertEqual(packet[10:40].split(b"\0", 1)[0], b"PacketProbe")
        self.assertEqual(packet[70:74], bytes((1, 30, 25, 25)))
        self.assertEqual(struct.unpack_from(">H", packet, 90)[0], 7)
        self.assertEqual(struct.unpack_from(">I", packet, 96)[0], 0x7F000001)

    def test_packet_stream_splits_fixed_and_variable_packets(self):
        variable = b"\x1c\x00\x07abcd"
        fixed = b"\x22\x05\x07"
        packets = split_packet_stream(variable + fixed)

        self.assertEqual(
            [(packet.offset, packet.command, packet.data) for packet in packets],
            [(0, 0x1C, variable), (len(variable), 0x22, fixed)],
        )

    def test_packet_stream_rejects_unknown_commands(self):
        with self.assertRaises(PacketStreamError):
            split_packet_stream(b"\xfe")

    def test_packet_stream_rejects_truncated_fixed_and_variable_packets(self):
        with self.assertRaises(PacketStreamError):
            split_packet_stream(b"\x1b" + bytes(10))
        with self.assertRaises(PacketStreamError):
            split_packet_stream(b"\x1c\x00")
        with self.assertRaises(PacketStreamError):
            split_packet_stream(b"\x1c\x00\x08abc")

    def test_packet_stream_allows_only_trailing_partial_packet(self):
        complete = b"\x22\x00\x00"
        partial = complete + b"\x1c\x00\x08abc"
        packets = split_packet_stream(partial, allow_truncated=True)

        self.assertEqual(len(packets), 1)
        self.assertEqual(packets[0].data, complete)

    def test_huffman_decodes_concatenated_server_frames(self):
        first = b"\x22\x01\x41"
        second = b"\x22\x02\x41"

        encoded = huffman_compress(first) + huffman_compress(second)

        self.assertEqual(huffman_decompress(encoded), first + second)

    def test_uncompressed_gump_layout_text_and_label(self):
        gump = parse_gump_dialog(recorded_gump_packet())
        self.assertEqual(
            (gump.serial, gump.context, gump.x, gump.y),
            (0x01020304, 0x05060708, 10, 20),
        )
        self.assertEqual(gump.texts, ("Choose class",))
        self.assertEqual(gump.find_button(button_id=7).page, 0)
        self.assertEqual(gump.find_button(label="choose class").button_id, 7)

    def test_compressed_gump_decodes_the_same_layout(self):
        gump = parse_gump_dialog(recorded_gump_packet(0xDD))
        self.assertEqual(gump.command, 0xDD)
        self.assertEqual(gump.texts, ("Choose class",))
        self.assertEqual(gump.find_button(label="class").button_id, 7)

    def test_gump_reply_round_trip_includes_switches_and_unicode_text(self):
        packet = make_gump_reply(
            0x01020304,
            0x05060708,
            7,
            switches=(11, 12),
            texts=((3, "Grüß"),),
        )
        reply = parse_gump_reply(packet)
        self.assertEqual(reply.serial, 0x01020304)
        self.assertEqual(reply.context, 0x05060708)
        self.assertEqual(reply.button_id, 7)
        self.assertEqual(reply.switches, (11, 12))
        self.assertEqual(reply.texts[0].text, "Grüß")

    def test_gump_parser_rejects_bad_lengths(self):
        packet = recorded_gump_packet()
        with self.assertRaises(GumpPacketError):
            parse_gump_dialog(packet[:-1])
        with self.assertRaises(GumpPacketError):
            parse_gump_dialog(packet[:1] + b"\x00\x01" + packet[3:])

    def test_skill_request_uses_extended_command_0x12(self):
        packet = make_skill_use(42)
        self.assertEqual(packet[:4], b"\x12\x00\x09\x24")
        request = parse_skill_use(packet)
        self.assertEqual(request.skill_id, 42)
        self.assertEqual(request.arguments, ("0",))


class FixtureOnlyProtocolAssertionTests(unittest.TestCase):
    class FakeSocket:
        def sendall(self, packet):
            pass

        def close(self):
            pass

    def _run_character_create(self, skip_fixture_tests):
        result = TestResult()
        sock = self.FakeSocket()
        with (
            patch("test_suite.game_connect", return_value=(sock, 1)),
            patch("test_suite.make_char_create", return_value=b"create"),
            patch("test_suite.recv_until_game_start", return_value=b"response"),
            patch("test_suite._drain_game_socket", return_value=b"response"),
            patch("test_suite.decode_game_response", side_effect=lambda data: data),
            patch("test_suite.find_start_packet", return_value=(0, b"start")),
            patch(
                "test_suite._find_system_message",
                side_effect=(
                    []
                    if skip_fixture_tests
                    else ["SPHERE_NEWBIE_MAGERY 1", "SPHERE_NEWBIE_RESIST 1"]
                ),
            ) as find_message,
        ):
            test_char_create(
                "localhost",
                2593,
                2593,
                result,
                skip_fixture_tests=skip_fixture_tests,
            )
        return result, find_message

    def test_imported_world_mode_skips_only_the_two_fixture_markers(self):
        result, find_message = self._run_character_create(skip_fixture_tests=True)

        self.assertEqual(result.passed, 1)
        self.assertEqual(result.failed, 0)
        self.assertEqual(result.skipped_fixture_only, 2)
        find_message.assert_not_called()

        output = io.StringIO()
        with redirect_stdout(output):
            self.assertTrue(result.summary())
        self.assertIn(
            "Results: 1/1 passed, 2 skipped (fixture-only), 0 failed",
            output.getvalue(),
        )

    def test_fixture_mode_still_checks_both_markers(self):
        result, find_message = self._run_character_create(skip_fixture_tests=False)

        self.assertEqual(result.passed, 3)
        self.assertEqual(result.failed, 0)
        self.assertEqual(result.skipped_fixture_only, 0)
        self.assertEqual(find_message.call_count, 2)


if __name__ == "__main__":
    unittest.main()
