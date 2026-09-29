#!/usr/bin/env python3
"""Unit tests for the stock-compatible log normalizer and diff."""

from __future__ import annotations

import unittest

from logdiff import diff_logs, normalize_line


class LogDiffTests(unittest.TestCase):
    def test_golden_levels_and_script_context(self):
        rows = {
            "12:00:DEBUG:(quests/start.scp,7)acct='alice' from 192.0.2.9":
                "DEBUG:(script,7)acct='<value>' from <ip>",
            "12:00:INFO:Server started on '127.0.0.1' port 2593":
                "INFO:Server started on '<value>' port <n>",
            "12:00:WARNING:(items.scp,19)Unknown item TYPE 0x1234":
                "WARNING:(script,19)Unknown item TYPE <hex>",
            "12:00:ERROR:Account 'alice' has 3 characters":
                "ERROR:Account '<value>' has <n> characters",
            "12:00:CRITICAL:Save 'sphereworld.scp' FAILED code -7":
                "CRITICAL:Save '<value>' FAILED code <n>",
        }
        for line, expected in rows.items():
            with self.subTest(line=line):
                self.assertEqual(normalize_line(line), expected)

    def test_plain_stock_events_are_info(self):
        self.assertEqual(
            normalize_line("Loading 'scripts\\sphere.scp'"),
            "INFO:Loading '<value>'",
        )

    def test_client_socket_prefix_is_run_specific(self):
        self.assertEqual(
            normalize_line("15:01:FC:Login 'alice'"),
            "INFO:Login '<value>'",
        )
        self.assertEqual(
            normalize_line("15:01:0c:Login 'alice'"),
            "INFO:Login '<value>'",
        )

    def test_diff_reports_only_classes_and_count_ratios(self):
        linux = [
            "12:00:ERROR:bad item 7",
            "12:01:ERROR:bad item 8",
            "12:02:INFO:Server started on '127.0.0.1' port 2593",
        ]
        windows = [
            "12:00:ERROR:bad item 9",
            "12:02:INFO:Server started on '10.0.0.1' port 2593",
            "12:03:WARNING:legacy warning",
        ]
        result = diff_logs(linux, windows)
        self.assertEqual(result["only_linux"], {})
        self.assertEqual(result["only_windows"], {"WARNING:legacy warning": 1})
        self.assertEqual(result["ratios"]["ERROR:bad item <n>"], {"linux": 2, "windows": 1, "ratio": 2.0})


if __name__ == "__main__":
    unittest.main()
