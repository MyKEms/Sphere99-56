# Sphere99-56 — SphereServer 0.99 Reconstruction

Reconstructed source code for **SphereServer 0.99** — a game server emulator for
Ultima Online 2D, originally developed by Menasoft (Dennis Robinson). The 0.99 branch
was proprietary and never officially open-sourced. This project aims to reconstruct
a fully functional 0.99-compatible server from available partial sources.

## Background

SphereServer had two parallel development branches that are **not sequential versions**:

```
1998  GrayWorld (Dennis Robinson / Menasoft)
        │
        ├─→ 0.55 → 0.56a → 0.56b → 0.56d    (community, open-source)
        │
        └─→ 0.99a → 0.99f → 0.99z8 → 0.99zl  (Menasoft, proprietary, closed)
```

The **0.56 line** is actively maintained by the community as [SphereServer Source-X](https://github.com/Sphereserver/Source-X).

The **0.99 line** died when Menasoft stopped development (~2003). Several UO shards
still run on 0.99 binaries with no source code to modify or fix bugs. **Scripts between
0.99 and 0.56 are fundamentally incompatible** — different expression syntax, trigger
systems, dialog APIs, and variable mechanisms.

This project reconstructs the 0.99 source from:
- Partial 0.99f source from [Sphereserver/Source-Archive](https://github.com/Sphereserver/Source-Archive)
- Gap-filling with compatible code from [JakubLinhart/Sphere99-56](https://github.com/JakubLinhart/Sphere99-56)
- Linux/GCC port and base library implementations (this fork)

## Current Status

| Metric | Value |
|--------|-------|
| Compile errors | **0** |
| Link errors | **0** |
| Binary | `sphere99svr` — 1.9 MB, 32-bit ELF (Linux x86) |
| Source files | 146 (.cpp + .h), ~97,500 lines |
| Stub functions remaining | 329 |
| Status | Compiles and links, not yet functional |

The server compiles and links but is not yet functional — 329 core functions are
stubbed with placeholder implementations. See [Development Roadmap](#development-roadmap).

## Building

```bash
# Prerequisites (Ubuntu/Debian)
sudo apt-get install -y gcc-multilib g++-multilib make

# Build
make            # produces sphere99svr (32-bit ELF binary)
make clean      # removes build artifacts
```

## Project Structure

```
spherelib/          Base library (strings, files, arrays, sockets, threads)
SphereCommon/       Shared UO data structures (maps, tiles, crypto, regions)
SphereAccount/      Account management
SphereSvr/          Main server (game logic: characters, items, clients, world)
Makefile            GNU Make build system (Linux/GCC)
CLAUDE.md           Detailed technical documentation
```

## Development Roadmap

### Goal: Complete 0.99 Server Reconstruction

Finish implementing the 329 remaining stubs to create a fully functional SphereServer
0.99 compatible with existing 0.99 shard scripts and save data.

**Implementation priority:**

1. **Script parser** (`CScript`) — load and parse .scp script files
2. **Expression evaluator** (`CExpression`, `CGVariant`) — including `<?...?>` escaped macro syntax unique to 0.99
3. **Resource loader** (`CResourceDef`) — ITEMDEF, CHARDEF, SPELL, SKILL definitions
4. **Script executor** (`CScriptExecContext`) — trigger and event handling
5. **Extended trigger support** — combat triggers like `@BeforeSwing`, `@AfterSwing`, `@finalBlow` used by many 0.99 shards
6. **Client crypto** (`CCryptBase`) — UO client login encryption
7. **Network layer** (`CGSocket`) — accept client connections
8. **World persistence** (`CWorld`) — save/load game state

### Why not migrate to 0.56d?

Analysis confirmed that migrating existing 0.99 shard scripts to 0.56d is
**not feasible** for established shards:

- `<?...?>` escaped macro syntax (0.99-only) has no 0.56d equivalent
- `argo.` dialog construction API is completely different in 0.56d
- Expression evaluation semantics differ (right-to-left without operator precedence in 0.99)
- Custom combat triggers don't exist in standard 0.56d
- Community consensus: migration is "practically impossible" for large script bases
- The 0.99 and 0.56 branches diverged too far to be compatible

## Contributing

Contributions welcome! All changes go through Pull Requests with code review.

When implementing stubs, use [SphereServer 0.56d source](https://github.com/SphereServer/Source)
as reference, but adapt to the 0.99 class interfaces (they differ significantly).

## References

| Resource | Description |
|----------|-------------|
| [Sphereserver/Source-Archive](https://github.com/Sphereserver/Source-Archive) | Partial 0.99f source code |
| [JakubLinhart/Sphere99-56](https://github.com/JakubLinhart/Sphere99-56) | Original reconstruction effort |
| [SphereServer Source-X](https://github.com/Sphereserver/Source-X) | Active 0.56d+ development (reference only) |
| [SphereCommunity](https://www.sphereserver.com/) | Community forums and documentation |
| [Sphere99.VsCode](https://github.com/uoinfusion/Sphere99.VsCode) | VS Code extension for 0.99 script editing |

## License

Server engine code derived from sources published under [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0)
by the [Sphereserver](https://github.com/Sphereserver) organization.

Original code copyright Menace Software (www.menasoft.com).
