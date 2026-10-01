#!/usr/bin/env python3
"""Test the fixture runner's fail-closed port ownership check."""

from __future__ import annotations

import socket

from port_guard import assert_port_free


def main() -> int:
    listener = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    listener.bind(("127.0.0.1", 0))
    listener.listen(1)
    port = listener.getsockname()[1]
    try:
        try:
            assert_port_free("occupied-port", port)
        except RuntimeError as error:
            if "occupied-port" not in str(error) or str(port) not in str(error):
                raise AssertionError(f"occupied port error was not descriptive: {error}")
        else:
            raise AssertionError("occupied port was accepted")
    finally:
        listener.close()

    assert_port_free("released-port", port)
    print("fixture runner port guard passed: occupied ports are rejected and free ports bind")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
