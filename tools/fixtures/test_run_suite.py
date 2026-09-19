#!/usr/bin/env python3
"""Unit tests for the synthetic fixture server shutdown gate."""

from __future__ import annotations

import subprocess
import unittest
from unittest.mock import Mock

from run_suite import (
    newbie_load_failures,
    shutdown_failures,
    stop_server,
    unknown_keyword_failures,
)


class ShutdownFailuresTests(unittest.TestCase):
    def test_clean_shutdown_passes(self) -> None:
        self.assertEqual(shutdown_failures(0, "server stopped cleanly\n"), [])

    def test_nonzero_server_exit_fails(self) -> None:
        failures = shutdown_failures(-6, "server stopped\n")
        self.assertTrue(any("status -6" in failure for failure in failures))

    def test_ubsan_output_fails_even_with_zero_exit(self) -> None:
        log = "SphereSvr/CContain.cpp:86: runtime error: invalid downcast\n"
        failures = shutdown_failures(0, log)
        self.assertTrue(any("sanitizer diagnostic" in failure for failure in failures))
        self.assertTrue(any("runtime error:" in failure for failure in failures))

    def test_asan_output_fails_even_with_zero_exit(self) -> None:
        log = "==42==ERROR: AddressSanitizer: heap-use-after-free\n"
        failures = shutdown_failures(0, log)
        self.assertTrue(any("sanitizer diagnostic" in failure for failure in failures))

    def test_missing_exit_status_fails_closed(self) -> None:
        failures = shutdown_failures(None, "")
        self.assertTrue(any("status was not captured" in failure for failure in failures))


class NewbieLoadFailuresTests(unittest.TestCase):
    def test_no_invalid_newbie_sections_passes(self) -> None:
        self.assertEqual(newbie_load_failures("loaded scripts\n"), [])

    def test_invalid_newbie_section_fails_with_source_line(self) -> None:
        line = "[ERROR] Invalid NEWBIE block index 'MAGERY'"
        failures = newbie_load_failures(line)
        self.assertTrue(any("1 invalid NEWBIE" in failure for failure in failures))
        self.assertTrue(any(line in failure for failure in failures))

    def test_expected_unknown_newbie_section_is_reported(self) -> None:
        line = "[ERROR] Invalid NEWBIE block index 'SYNTHETIC_UNKNOWN_SKILL'"
        self.assertEqual(
            newbie_load_failures(line, ("SYNTHETIC_UNKNOWN_SKILL",)), []
        )

    def test_unexpected_newbie_section_still_fails(self) -> None:
        line = "[ERROR] Invalid NEWBIE block index 'MAGERY'"
        failures = newbie_load_failures(line, ("SYNTHETIC_UNKNOWN_SKILL",))
        self.assertTrue(any("expected ['SYNTHETIC_UNKNOWN_SKILL']" in f for f in failures))
        self.assertTrue(any(line in failure for failure in failures))


class StopServerTests(unittest.TestCase):
    def test_returns_status_for_already_exited_server(self) -> None:
        process = Mock()
        process.poll.return_value = -6
        process.wait.return_value = -6

        self.assertEqual(stop_server(process), -6)
        process.terminate.assert_not_called()

    def test_returns_status_after_graceful_termination(self) -> None:
        process = Mock()
        process.poll.return_value = None
        process.wait.return_value = 0

        self.assertEqual(stop_server(process), 0)
        process.terminate.assert_called_once_with()
        process.wait.assert_called_once_with(timeout=10)

    def test_kills_server_after_graceful_shutdown_timeout(self) -> None:
        process = Mock()
        process.poll.return_value = None
        process.wait.side_effect = [
            subprocess.TimeoutExpired(cmd="sphere99svr", timeout=10),
            -9,
        ]

        self.assertEqual(stop_server(process), -9)
        process.terminate.assert_called_once_with()
        process.kill.assert_called_once_with()
        self.assertEqual(process.wait.call_count, 2)


class UnknownKeywordGateTests(unittest.TestCase):
    def test_empty_report_passes_empty_allowlist(self) -> None:
        report = {"distinct": 0, "total": 0, "overflow": 0, "entries": []}
        self.assertEqual(unknown_keyword_failures(report, {"entries": []}), [])

    def test_unexpected_keyword_fails(self) -> None:
        report = {
            "distinct": 1,
            "total": 1,
            "overflow": 0,
            "entries": [{"kind": "get", "keyword": "MISSING", "count": 1}],
        }
        failures = unknown_keyword_failures(report, {"entries": []})
        self.assertTrue(any("unexpected unknown-keyword keys" in f for f in failures))

    def test_allowlisted_count_must_match(self) -> None:
        report = {
            "distinct": 1,
            "total": 2,
            "overflow": 0,
            "entries": [{"kind": "get", "keyword": "KNOWN_GAP", "count": 2}],
        }
        allowlist = {
            "entries": [{"kind": "get", "keyword": "KNOWN_GAP", "count": 1}]
        }
        failures = unknown_keyword_failures(report, allowlist)
        self.assertTrue(any("expected 1" in f for f in failures))

    def test_allowlisted_count_range_accepts_runtime_variation(self) -> None:
        report = {
            "distinct": 1,
            "total": 20,
            "overflow": 0,
            "entries": [{"kind": "trigger", "keyword": "@TIMER", "count": 20}],
        }
        allowlist = {
            "entries": [
                {
                    "kind": "trigger",
                    "keyword": "@TIMER",
                    "count_range": [1, 32],
                }
            ]
        }
        self.assertEqual(unknown_keyword_failures(report, allowlist), [])

    def test_allowlisted_count_range_rejects_out_of_range_count(self) -> None:
        report = {
            "distinct": 1,
            "total": 33,
            "overflow": 0,
            "entries": [{"kind": "trigger", "keyword": "@TIMER", "count": 33}],
        }
        allowlist = {
            "entries": [
                {
                    "kind": "trigger",
                    "keyword": "@TIMER",
                    "count_range": [1, 32],
                }
            ]
        }
        failures = unknown_keyword_failures(report, allowlist)
        self.assertTrue(any("expected count 1..32" in f for f in failures))

    def test_optional_allowlist_key_may_be_absent(self) -> None:
        report = {"distinct": 0, "total": 0, "overflow": 0, "entries": []}
        allowlist = {
            "entries": [
                {
                    "kind": "trigger",
                    "keyword": "@TIMER",
                    "count_range": [1, 32],
                    "optional": True,
                }
            ]
        }
        self.assertEqual(unknown_keyword_failures(report, allowlist), [])

    def test_overflow_fails_even_when_keys_are_allowlisted(self) -> None:
        report = {"distinct": 0, "total": 1, "overflow": 1, "entries": []}
        failures = unknown_keyword_failures(report, {"entries": []})
        self.assertTrue(any("overflowed" in f for f in failures))

    def test_boolean_summary_counters_are_rejected(self) -> None:
        report = {"distinct": False, "total": False, "overflow": False, "entries": []}
        failures = unknown_keyword_failures(report, {"entries": []})
        self.assertTrue(any("invalid unknown-keyword distinct" in f for f in failures))
        self.assertTrue(any("invalid unknown-keyword total" in f for f in failures))
        self.assertTrue(any("invalid unknown-keyword overflow" in f for f in failures))


if __name__ == "__main__":
    unittest.main()
