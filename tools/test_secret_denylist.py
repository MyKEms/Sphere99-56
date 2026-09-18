#!/usr/bin/env python3
"""Offline checks for the optional machine-local denylist support."""

import unittest

from check_secrets import compile_denylist, scan_external_content


class ExternalDenylistTests(unittest.TestCase):
    def test_literal_and_regex_patterns_are_case_insensitive(self):
        patterns = compile_denylist(
            [
                "# comments are ignored",
                "literal:Private-Marker.example",
                r"regex:^deployment-[0-9]+$",
            ]
        )
        findings = scan_external_content(
            "fixture.txt",
            "safe\nprivate-marker.EXAMPLE\ndeployment-42\npublic\n",
            patterns,
        )
        self.assertEqual([finding[0] for finding in findings], [2, 3])
        self.assertEqual(findings[0][2], "[redacted denylist match]")

    def test_empty_and_comment_lines_do_not_match(self):
        patterns = compile_denylist(["", "   ", "# no pattern"])
        self.assertEqual(scan_external_content("fixture.txt", "anything\n", patterns), [])


if __name__ == "__main__":
    unittest.main()
