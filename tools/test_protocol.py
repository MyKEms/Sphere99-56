#!/usr/bin/env python3
"""Offline regression checks for the public headless protocol helpers."""

import struct
import unittest

from uo_test_client import (
    make_char_create,
    make_char_play,
    make_charlist_req,
    make_login_packet,
    make_server_select,
)


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


if __name__ == "__main__":
    unittest.main()
