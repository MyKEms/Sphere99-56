# AGENTS.md — instructions for coding agents

**Read `CLAUDE.md` first.** Despite its name, it contains the project instructions for every agent. Its "Public repository boundary" section is absolute: this is a public repository, and private shard scripts, configuration, saves, accounts, production-derived data and credentials must never enter it.

## Procedures

| Task | Read |
|------|------|
| build + run the protocol suite | `.claude/skills/test/SKILL.md` |
| commit / push / PR / merge | `.claude/skills/ship/SKILL.md` |
| crash debugging, sanitizers | `README.md` (debug and ASan sections), `make debug`, `make asan` |
| CI-equivalent local run | `tools/fixtures/make_fixture.py` + `tools/fixtures/run_suite.py` (see `CLAUDE.md`) |

## Contributing workflow

- One change → one branch → one pull request. Never add commits to a branch whose PR is already merged.
- The PR body states the root cause, the fix and the test evidence (exact commands, pass counts, fixture type). A client-facing fix needs a test that fails without it.
- PR titles, bodies, commit messages and code comments describe the technical change only; they never reference external trackers, deployments or data.
- Merge with `gh pr merge <n> --rebase` after CI is green. Commit authors and committers use the noreply address; do not add `Co-Authored-By` trailers.
- Crash-safety and sanitizer findings come before new features.
