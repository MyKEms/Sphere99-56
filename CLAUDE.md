# Development Instructions — SphereServer 0.99 Engine

## Public repository boundary — absolute rule

This directory is the public GitHub engine repository. Never copy, import,
commit, push, upload, or quote private GitLab shard scripts, shard-specific
configuration, `save/`, `accounts/`, world backups, production-derived data,
or credentials here. If a change cannot be proven generic and public-safe,
keep it out of this repository.

The versioned `.githooks/pre-commit` guard and `tools/check_secrets.py` reject
sensitive paths and common credentials. Activate them with:

```bash
git config core.hooksPath .githooks
```

Never bypass the guard with `--no-verify`.

For deployment-specific identifiers, keep a machine-local denylist outside the
worktree and set `SPHERE_PRIVATE_DENYLIST`, or use `.git/info/private-denylist`.
Its non-empty lines are case-insensitive regexes; `literal:` forces literal
matching and `regex:` makes regex intent explicit. The pre-commit path scans
staged additions, `--all` scans tracked files, and `.githooks/commit-msg` also
checks commit messages. Never put private identifiers into the repository just
to test the denylist.

## Project goal

Reconstruct a usable, generic SphereServer 0.99 engine from the available
historical source material. Shard scripts, configuration, UO data files,
world saves, and account databases are external runtime inputs and are never
part of this public repository.

## Build

```bash
# Debian/Ubuntu
sudo apt-get install -y gcc-multilib g++-multilib make
make -j"$(nproc)"
make clean
```

The supported Linux target is a 32-bit i386 ELF binary. The code still uses
some historical 32-bit assumptions, so a successful 64-bit build is not a
replacement for the i386 build.

## Current validation status

- The public CI builds the engine with GCC multilib and checks the ELF type.
- `tools/test_protocol.py` is the dependency-free offline packet-fixture test.
- `tools/test_suite.py` contains thirteen integration checks. CI generates a
  synthetic disposable runtime fixture locally, including sparse map data and
  generic resource definitions; it creates accounts and characters, so never
  point it at a world you intend to keep.
- A real ClassicUO/TazUO client and a representative world remain separate
  compatibility tests. The public repository does not claim production
  compatibility from the headless suite alone.

## Source directories

```
spherelib/          common containers, strings, sockets, expressions
SphereCommon/       UO protocol, maps, tiles, crypto, regions
SphereAccount/      account model and persistence interfaces
SphereSvr/          server, clients, world, characters, items
tools/              headless protocol client and test utilities
```

## Known limitations

- The Linux runtime intentionally uses a single-threaded server loop while
  the historical Windows design had additional background threads.
- Several 0.99 script semantics and object-reference paths still need
  differential testing against known-good client/server behavior.
- Death/re-login, container interaction, NPC/combat edge cases, and some
  world-save paths require more coverage than the headless login fixture.
- Debugging 32-bit faults under qemu-i386 on Apple Silicon is limited. Use a
  native x86-64 Linux host for gdb, core files, ASan, and rr; keep all dumps
  local because they may contain runtime data.

## Protocol and runtime notes

- `sphere.ini` and the resource table may contain historical Windows paths;
  Linux runtime paths must use `/` and be supplied outside this repository.
- `SCPFILES` is the external script base directory. The public engine must
  remain usable with a disposable fixture without assuming a shard name.
- `CResourceObj::m_dwHashIndex` stores object/resource UIDs. Index `0` is the
  invalid value, so the UID table reserves slot zero and runtime objects start
  at a non-zero index.
- Virtual `s_PropSet`, `s_PropGet`, and `s_Method` signatures must match
  exactly. A mismatch silently dispatches to a base stub.

## Coding rules

- Keep the 32-bit Linux build at zero compile and link errors.
- Use `-fpermissive` only for historical source patterns that need it.
- Guard Windows-only APIs with `#ifdef _WIN32`; do not remove the original
  platform path just to make Linux compile.
- Use exact filename case in includes and Makefile entries.
- Add a focused offline or protocol test for each client-facing fix.
- Do not add `Co-Authored-By` trailers to commits in this repository.
- Never commit shard scripts, `.scp` data, saves, accounts, MULs, logs,
  private configuration, archives, keys, or core dumps.

## Testing and debugging

```bash
python3 -m py_compile tools/*.py
python3 tools/test_protocol.py
python3 tools/test_suite.py localhost 2593 --quick
python3 tools/check_secrets.py --all
```

The public CI path can be reproduced locally on Linux with:

```bash
python3 tools/fixtures/make_fixture.py /tmp/sphere99-fixture
python3 tools/fixtures/run_suite.py \
  /tmp/sphere99-fixture \
  --binary "$PWD/sphere99svr" \
  --repo "$PWD"
```

The fixture is synthetic and generated outside the repository; it must never
be replaced with client MULs, shard scripts, saves, accounts, or production
logs.

The integration suite must run against a disposable world. On Apple Silicon,
run the i386 binary in a Linux/amd64 VM or container for compatibility checks;
move crash debugging to native x86-64 Linux. Do not use production data to
make a test fixture and do not upload logs or cores.

## References

- [SphereServer Source Archive](https://github.com/Sphereserver/Source-Archive)
- [SphereServer 0.56 source](https://github.com/SphereServer/Source)
- [SphereServer Source-X](https://github.com/Sphereserver/Source-X)
- [UO packet documentation](https://modernuo.com/packets.html)

The 0.99 branch was proprietary and was never officially open-sourced. Keep
copyright and license provenance for every imported or reconstructed section
visible in the relevant source and project documentation.
