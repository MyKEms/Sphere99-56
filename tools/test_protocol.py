#!/usr/bin/env python3
"""Offline regression checks for the public headless protocol helpers."""

import struct
import unittest

from uo_huffman import compress as huffman_compress, decompress as huffman_decompress
from uo_test_client import (
    make_char_create,
    make_char_play,
    make_charlist_req,
    make_login_packet,
    make_server_select,
)
from uo_packets import PacketStreamError, split_packet_stream


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


if __name__ == "__main__":
    unittest.main()
