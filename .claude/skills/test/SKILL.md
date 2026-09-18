---
name: test
description: Build the engine and run the UO protocol test suite against a running server. Use after engine changes. Pass "full" for all tests (default is --quick).
allowed-tools: Bash(make *), Bash(python3 *), Bash(ss *), Bash(pgrep *), Bash(tail *)
---

# /test — Build and run the Sphere99 test suite

Paths are relative to the engine repo root. The server data directory
(sphere.ini, scripts/, muls/, save/, accounts/) is NOT part of this repo; its
location is `$SPHERE_DIR` (ask the user if unset — when this repo is a
submodule of a deployment repo, it is usually that repo's root).

1. Build: `make -j"$(nproc)"`. A build error is a failure — stop and report it.
2. Is a server running? `ss -tln | grep ':2593 '`.
   - If one is running from an older binary, ask before restarting it.
   - To start: `cp sphere99svr "$SPHERE_DIR/"`, then from `$SPHERE_DIR` run
     `tail -f /dev/null | ./sphere99svr > sphere99svr.log 2>&1 &` and poll the
     port for up to 10 minutes (a production-size save loads for minutes).
     Keep the log — never redirect it to /dev/null.
3. Run: `python3 tools/test_suite.py localhost 2593 --quick`
   (for `/test full`: without `--quick`).
4. Report pass/fail per test. On failures, include the last ~40 relevant lines
   of `$SPHERE_DIR/sphere99svr.log` (skip `OnTick phase` spam) and check whether
   the server process is still alive (`pgrep -x sphere99svr`).

Caveats:
- The tests create accounts and characters (AutoTest, GameEntry, ...). Never
  point them at a server running a production save you intend to keep.
- Many tests only detect "server died"; a PASS is weak evidence. Read the log.
