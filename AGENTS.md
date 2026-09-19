# AGENTS.md — instructions for coding agents

**Read `CLAUDE.md` first.** Despite its name, it contains the project instructions for every agent. Its "Public repository boundary" section is absolute: this is a public repository, and private shard scripts, configuration, saves, accounts, production-derived data and credentials must never enter it.

## Procedures

| Task | Read |
|------|------|
| build + run the protocol suite | `.claude/skills/test/SKILL.md` |
| commit / push / PR / merge | `.claude/skills/ship/SKILL.md` |
| crash debugging, sanitizers | `README.md` (debug and ASan sections), `make debug`, `make asan` |
| CI-equivalent local run | `tools/fixtures/make_fixture.py` + `tools/fixtures/run_suite.py` (see `CLAUDE.md`) |

## Coordination

- Work items are GitHub issues in this repository. The pinned issue **#5** holds the working agreement and the current priority queue. Read it, including its latest comments, before choosing work.
- One issue → one new branch (`fix/<issue>-<slug>`) → one PR. Never add commits to a branch whose PR is already merged.
- Every PR body states the root cause, the fix and the test evidence: exact commands, pass counts and fixture type. A client-facing fix needs a test that fails without it.
- Merge with `gh pr merge <n> --rebase` after CI is green. Commit authors and committers must use the noreply address; do not add `Co-Authored-By` trailers.
- Crash-safety and sanitizer findings come before new features.
