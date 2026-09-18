# Sphere99-56 — SphereServer 0.99 reconstruction

This repository contains a generic Linux port and reconstruction of the
SphereServer 0.99 engine for Ultima Online 2D. It contains source code and
developer tools only. Shard scripts, world saves, account databases, MUL data,
private configuration, and production logs are deliberately excluded.

## Status

The project is under active reconstruction and is not a production-ready
server distribution. The public CI verifies a 32-bit Linux build and a
dependency-free packet-fixture test. The integration harness exercises login,
character creation, game entry, and stability against a disposable external
runtime fixture; real-client and representative-world compatibility still
need to be tested separately.

## Public safety boundary

Before contributing, activate the versioned fail-closed hook:

```bash
git config core.hooksPath .githooks
```

The hook and CI reject credentials, private keys, runtime logs, archives, MULs,
`.scp` files, `save/`, `accounts/`, and shard script directories. Never bypass
them with `--no-verify`.

## Building

```bash
# Debian/Ubuntu
sudo apt-get install -y gcc-multilib g++-multilib make python3
make -j"$(nproc)"
make clean
```

The supported target is a 32-bit i386 ELF binary named `sphere99svr`.

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
```

## Layout

```
spherelib/          base library
SphereCommon/       UO protocol and world-data structures
SphereAccount/      account management
SphereSvr/          server and game logic
tools/              headless protocol client, fixtures, and checks
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
