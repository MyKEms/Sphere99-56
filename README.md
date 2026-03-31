# Sphere99-56 — SphereServer 0.99 reconstruction

This repository contains a generic Linux port and reconstruction of the
SphereServer 0.99 engine for Ultima Online 2D. It contains source code and
developer tools only. Shard scripts, world saves, account databases, MUL data,
private configuration, and production logs are deliberately excluded.

## Status

The project is under active reconstruction and is not a production-ready
server distribution. The public CI verifies a 32-bit Linux build, offline
packet fixtures, and the headless protocol suite against a generated,
redistributable synthetic runtime. Real-client and representative-world
compatibility still need to be tested separately.

## Public safety boundary

Before contributing, activate the versioned fail-closed hook:

```bash
git config core.hooksPath .githooks
```

The hook and CI reject credentials, private keys, runtime logs, archives, MULs,
`.scp` files, `save/`, `accounts/`, and shard script directories. Never bypass
them with `--no-verify`.

Deployments may add private, machine-local identifiers without putting them in
this repository. Set `SPHERE_PRIVATE_DENYLIST` to a file outside the worktree,
or create `.git/info/private-denylist`. Each non-empty line is a
case-insensitive regular expression; use `literal:value` for a literal string
and `regex:value` when the intent should be explicit. The pre-commit scan
checks staged additions, `--all` checks all tracked files, and the versioned
commit-message hook checks commit messages too. Denylist matches are redacted
from diagnostics.

## Building

```bash
# Debian/Ubuntu
sudo apt-get install -y gcc-multilib g++-multilib make python3
make -j"$(nproc)"
make clean
```

The supported target is a 32-bit i386 ELF binary named `sphere99svr`.

### Debug and sanitizer builds

Use native x86-64 Linux for debugging memory safety. The debug and sanitizer
targets use separate object trees, so they cannot mix objects with the legacy
i386 build:

```bash
make clean
make debug                         # build/debug/sphere99svr

make clean
make asan                          # build/asan/sphere99svr
ASAN_OPTIONS=quarantine_size_mb=64:malloc_context_size=8:detect_leaks=0:abort_on_error=1 \
  ./build/asan/sphere99svr
```

`make debug` uses `-O0 -g3 -D_GLIBCXX_ASSERTIONS -fno-omit-frame-pointer`.
`make asan` uses AddressSanitizer plus UndefinedBehaviorSanitizer with
`-O1 -g -fno-omit-frame-pointer`. Both targets disable the server's legacy
`siglongjmp` crash recovery so GDB and sanitizers receive the original fault;
the custom allocator continues to use `malloc`, which ASan tracks normally.

Crash recovery is opt-in everywhere: normal builds leave SIGSEGV, SIGBUS, and
SIGABRT with the operating system, debugger, or sanitizer. To build the
legacy guarded recovery mode explicitly, use `make recover`; it produces
`build/recover/sphere99svr` with `SPHERE_SEGV_RECOVERY`. That mode only guards
SIGSEGV/SIGBUS and never intercepts SIGABRT. Do not use it while debugging a
fault you need GDB or ASan to report at the original instruction.

The sanitizer target is a build-only CI check. Run it against a disposable
fixture when adding runtime coverage, never against production `save/` or
`accounts/` data. The i386 runtime and its `-m32` default build are not valid
ASan/GDB environments under qemu user emulation. On Apple Silicon, use a
native x86-64 Linux host or an x86-64 Linux VM/container for sanitizer work;
the ARM host and emulated i386 runtime are useful for ordinary protocol tests,
not reliable sanitizer diagnostics.

## Testing

Offline packet and helper checks need no server:

```bash
python3 -m py_compile tools/*.py
python3 tools/test_protocol.py
```

The integration suite needs an external disposable runtime directory containing
the server configuration, scripts, UO MUL files, empty/save fixture, and
accounts fixture. It creates test accounts and characters, so never run it
against a world you intend to keep:

```bash
python3 tools/test_suite.py localhost 2593 --quick
python3 tools/test_suite.py localhost 2593
# Imported script sets lack the fixture-only trigger hooks used by the last test.
python3 tools/test_suite.py localhost 2593 --skip-fixture-tests
```

The full synthetic-fixture run reports 15 assertions. `--skip-fixture-tests`
omits the two assertions that require the fixture's custom script hooks; the
remaining protocol checks still create accounts and characters, so use only a
disposable runtime in either mode.

For a completely self-contained Linux smoke test, generate the synthetic
fixture and run the server plus the full suite. The generated directory is
outside the repository and contains no client or shard data:

```bash
python3 tools/fixtures/make_fixture.py /tmp/sphere99-fixture
make -j"$(nproc)"
python3 tools/fixtures/run_suite.py \
  /tmp/sphere99-fixture \
  --binary "$PWD/sphere99svr" \
  --repo "$PWD"

# Include the bounded client-lifetime soak (ASan/UBSan CI runs 25 cycles).
python3 tools/fixtures/run_suite.py \
  /tmp/sphere99-fixture \
  --binary "$PWD/sphere99svr" \
  --repo "$PWD" \
  --lifetime-soak 25
```

The CI fixture job uses the same flow, polls the login socket with a bounded
startup timeout, and uploads only the disposable server log if the test fails.
The lifetime soak alternates graceful and abrupt disconnects, validates the
game-start packet, sends bounded movement traffic, and checks that the login
socket remains available after every cycle. See
[`docs/lifetime-model.md`](docs/lifetime-model.md) for the ownership boundary
and its deliberate out-of-scope areas.

The loader has a fail-closed save guard. If a world, chars, or statics file
skips a section or fails to parse, the server logs a critical summary and
refuses autosave and plain `SAVE`. Review the source first; an administrator
may explicitly acknowledge the risk with `SAVE FORCE`. The regression test
uses a temporary broken world file and verifies that no save file is created:

```bash
make load-safety-test
build/load-safety/load_safety_test
```

## Runtime unresolved-keyword report

Reporting is disabled unless the `[SPHERE]` section of `sphere.ini` sets a
report path:

```ini
UNKNOWNKEYWORDREPORT=logs/unknown-keywords.json
```

The server writes the report on a clean shutdown. A path ending in `.csv`
selects CSV; any other extension selects JSON. An administrator can write the
current snapshot on demand with `SERV.UNKNOWNREPORT`. Writing a snapshot does
not clear the collected counts.

Each entry groups a normalized keyword by kind (`get`, `set`, `method`,
`function`, `trigger`, or `rejected`) and includes its count and first source
file, line, and object type. `rejected` records unresolved dispatches that
return a bad-argument or invalid-result code. Dotted suffixes and numeric
indexes are grouped, so `TAG.name` becomes `TAG.*` and `ARGV[3]` becomes
`ARGV[]`. Collection retains
at most 1,024 distinct keys; later new keys increment the `overflow` counter.
The JSON root includes `distinct`, `total`, `overflow`, and `entries` fields.
CSV output uses the same entry columns and ends with `overflow` and `total`
summary rows.

## Layout

```
spherelib/          base library
SphereCommon/       UO protocol and world-data structures
SphereAccount/      account management
SphereSvr/          server and game logic
tools/              headless protocol client, synthetic fixtures, and checks
Makefile            GNU Make build
```

At runtime, paths such as `scripts/`, `save/`, `accounts/`, and `muls/` are
provided by the deployment environment, not committed here.

## Background

SphereServer had separate 0.56 and proprietary 0.99 development lines. The
0.99 source was not officially open-sourced; this project combines available
historical material with a Linux/GCC port and compatibility work. The 0.99
script language, triggers, dialogs, and persistence formats are not assumed to
be interchangeable with 0.56.

## Roadmap

- complete differential coverage for 0.99 expression and script semantics;
- improve object references, triggers, dialogs, combat, and re-login paths;
- add packet-level regression fixtures for client-facing behavior;
- validate world loading and saving with disposable representative fixtures;
- move crash debugging to native x86-64 Linux while keeping i386 runtime CI.

## References

| Resource | Description |
|---|---|
| [SphereServer/Source-Archive](https://github.com/Sphereserver/Source-Archive) | Partial historical source material |
| [SphereServer/Source](https://github.com/SphereServer/Source) | 0.56 reference implementation |
| [SphereServer/Source-X](https://github.com/Sphereserver/Source-X) | Active community reference |
| [ModernUO packet documentation](https://modernuo.com/packets.html) | UO protocol reference |

## License

See [LICENSE](LICENSE). Preserve the original copyright and license notices
when modifying reconstructed source.
