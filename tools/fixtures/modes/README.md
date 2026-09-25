# Synthetic fixture modes

Each Python module in this directory registers one named fixture mode.  A
module owns its test invocation, a private synthetic-ID block, and the
arguments needed by the compatibility writer.  `fixture_cases.py` discovers
the modules at runtime, so adding a case does not edit a shared case table or
CI command list.

The ID allocator is deliberately deterministic: a mode can request IDs only
from its own 0x1000-entry block.  Existing fixture bytes remain unchanged while
this registry is introduced; the legacy `make_fixture.py` flags remain
available to callers outside CI.
