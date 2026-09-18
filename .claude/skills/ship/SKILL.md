---
name: ship
description: Ship a public Sphere engine change through mandatory tests, secret-boundary checks, a pushed branch, and an owner-authorized PR merge.
---

# Ship the public engine

Use this skill for public engine commit, push, PR, retarget, and merge work. Keep the repository self-contained and generic: do not add private deployment paths, internal infrastructure names, credentials, runtime data, or generated artifacts.

## Mandatory gates

Before committing, inspect the complete staged diff and run:

```sh
git diff --check
python3 tools/check_secrets.py --all
python3 -m py_compile tools/*.py
python3 tools/test_protocol.py
make -j"$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 2)"
```

Run ShellCheck on repository shell scripts using the documented host or container command. Run the integration suite only with disposable test fixtures. If the native host cannot reproduce the supported 32-bit build, use Linux/CI and record that evidence. A missing tool is a failed gate until the documented alternative has been run.

Stage explicit paths, run `.githooks/pre-commit`, and verify that no credentials, private keys, generated runtime data, account/world data, dumps, logs, or private infrastructure references entered the public diff. Never use `--no-verify` or suppress the scanner.

## Owner merge policy

The repository owner has authorized bypassing the required-review policy for their own shipping requests. After the expected PR base/head are verified and every technical check is green, merge with:

```sh
gh pr merge <number> --repo MyKEms/Sphere99-56 --admin --rebase
```

Use this admin bypass when the owner invokes shipping, even if the PR is blocked only by required review. It bypasses review only; never bypass CI, lint, secret/sensitive-data checks, hooks, conflicts, a changed/unexpected head, or an unreviewed diff. Use rebase merges, not merge commits: a merge commit is authored with the GitHub account's primary e-mail, while a rebase merge keeps only the PR's commits (which must all be authored with the noreply address — check `git log --format='%ae %ce' origin/master..<head>` before merging). Verify the merged SHAs, and report the resulting commits and checks.
