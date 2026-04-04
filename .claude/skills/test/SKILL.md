---
name: test
description: Build server and run the UO protocol test suite
allowed-tools: Bash(make *), Bash(python3 *), Bash(ss *), Bash(kill *), Bash(cp *)
---

# /test — Build and run Sphere99 test suite

1. Build: `cd /workspace/Erebor/Migrace/Sphere99-56 && make -j4`
2. Deploy: `cp sphere99svr /workspace/Erebor/sphere/`
3. Check if server running: `ss -tlnp | grep 2593`
4. If not running, start: `cd /workspace/Erebor/sphere && tail -f /dev/null | ./sphere99svr > /dev/null 2>/dev/null &` and wait up to 3 minutes
5. Run quick tests: `python3 tools/test_suite.py localhost 2593 --quick`
6. Report results

For full suite (without --quick): `/test full`
