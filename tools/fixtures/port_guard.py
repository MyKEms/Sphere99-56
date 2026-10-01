"""Fail-closed checks for fixture ports before a child server is launched."""

from __future__ import annotations

import socket


def assert_port_free(case_name: str, port: int, host: str = "127.0.0.1") -> None:
    """Reject a fixture case when another listener already owns its port."""

    probe = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        probe.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        probe.bind((host, port))
    except OSError as error:
        raise RuntimeError(
            f"refusing fixture case {case_name!r}: {host}:{port} is already in use; "
            "the runner will not connect to a server it did not start"
        ) from error
    finally:
        probe.close()
