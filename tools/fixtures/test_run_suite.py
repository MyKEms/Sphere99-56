#!/usr/bin/env python3
"""Unit tests for the synthetic fixture server shutdown gate."""

from __future__ import annotations

import subprocess
import unittest
from unittest.mock import Mock

from run_suite import shutdown_failures, stop_server


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


if __name__ == "__main__":
    unittest.main()
