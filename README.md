# Sphere99-56 — SphereServer 0.99 Reconstruction

Reconstructed source code for **SphereServer 0.99** — a game server emulator for
Ultima Online 2D, originally developed by Menasoft (Dennis Robinson). The 0.99 branch
was proprietary and never officially open-sourced. This project aims to reconstruct it
for the [Erebor UO shard](https://uoerebor.cz).

## Background

SphereServer had two parallel development branches that are **not sequential versions**:

```
1998  GrayWorld (Dennis Robinson / Menasoft)
        │
        ├─→ 0.55 → 0.56a → 0.56b → 0.56d    (community, open-source)
        │
        └─→ 0.99a → 0.99f → 0.99z8 → 0.99zl  (Menasoft, proprietary, closed)
                                 ↑
                            Erebor shard
```

Scripts between 0.99 and 0.56 are **fundamentally incompatible** — different expression
syntax (`<?...?>` vs `<...>`), different trigger systems, different dialog APIs, different
variable mechanisms. Migration between branches requires near-complete script rewrites.

This project reconstructs the 0.99 source from:
- Partial 0.99f source from [Sphereserver/Source-Archive](https://github.com/Sphereserver/Source-Archive)
- Gap-filling with 0.56 compatible code from [JakubLinhart/Sphere99-56](https://github.com/JakubLinhart/Sphere99-56)
- Linux/GCC port and stub implementations (this fork)

## Current Status

| Metric | Value |
|--------|-------|
| Compile errors | **0** |
| Link errors | **0** |
| Binary | `sphere99svr` — 1.9 MB, 32-bit ELF (Linux x86) |
| Source files | 146 (.cpp + .h), ~97,500 lines |
| Stub functions remaining | 329 |
| Status | Compiles and links, not yet functional |

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
CLAUDE.md           Detailed technical documentation and development instructions
```

## Development Roadmap

### Primary Goal: Complete 0.99 Reconstruction

Branch: `variant-a/sphere99-reconstruction`

Finish implementing the 329 remaining stubs to create a fully functional 0.99 server
compatible with the Erebor shard's scripts and save data.

**Implementation priority:**
1. Script parser (`CScript`) — load .scp script files
2. Expression evaluator (`CExpression`, `CGVariant`) — including `<?...?>` escaped macro syntax
3. Resource loader (`CResourceDef`) — ITEMDEF, CHARDEF, SPELL definitions
4. Script executor (`CScriptExecContext`) — trigger and event handling
5. **Custom Erebor triggers** — 12+ combat triggers compiled into the original binary
6. Client crypto (`CCryptBase`) — UO client login encryption
7. Network layer (`CGSocket`) — accept client connections
8. World persistence (`CWorld`) — save/load game state

**Custom triggers to implement:**
```
@BeforeSwing, @AfterSwing, @beforeGetSwing, @afterGetSwing,
@beforeDoEffect, @afterDoEffect, @beforeGetEffect,
@finalBlow, @DrinkingPotion, @playerKill, @npckill,
@HitMiss, @HitTry, @itemDAMAGE
```

### Secondary: Migration Assessment (on hold)

Branch: `variant-b/migrate-to-056d`

Initially considered migrating to the functional [SphereServer 0.56d](https://github.com/SphereServer/Source).
Analysis revealed this is **not feasible** for the Erebor shard due to:

- **510,000 lines** of heavily customized scripts
- **2,107** uses of `<?...?>` escaped macro syntax (0.99-only)
- **15,000+** `argo.` dialog API calls (incompatible with 0.56d)
- **12+ custom combat triggers** compiled into the server binary
- **4,377** `argv()/argvcount` calls with different parsing semantics
- **383 event blocks** built on the custom trigger pipeline
- Community consensus: migration is "practically impossible" for large shards

## Erebor Shard Overview

The target shard is a deeply customized Czech-language Ultima Online server:

| Metric | Value |
|--------|-------|
| Script files | 526 (.scp) |
| Total script lines | ~510,000 |
| ITEMDEFs | 14,760 |
| CHARDEFs | 1,517 |
| Custom functions | 3,580 |
| Dialog definitions | 758 |
| Quest files | 130+ (117K lines) |
| Event blocks | 383 |
| Language | Czech (with English technical terms) |

**Key custom systems:**
- Custom combat engine with 12+ proprietary triggers and multi-stage damage pipeline
- Class/profession system (10 classes: Warrior, Ranger, Thief, Priest, Shaman, Mage, Mystik, Necromancer, Crafter)
- Guardian NPC system — invisible immortal character maintaining region timers
- Conquest/warfare between Gondor and Mordor
- 9-tier material system (copper through mithril)
- Custom quest framework with 130+ quests
- Dynamic region system with replacement stones
- Middle-earth themed locations (Minas Tirith, Barad-dur, Moria, etc.)
- Custom data structures (FIFO, LIFO, priority queue)
- AFK detection with captcha system

## Contributing

All changes go through Pull Requests with code review.

Branch structure:
- `master` — stable Linux build base (protected)
- `variant-a/sphere99-reconstruction` — active development (protected)
- `variant-b/migrate-to-056d` — on hold, kept for reference (protected)

## References

| Resource | Description |
|----------|-------------|
| [Sphereserver/Source-Archive](https://github.com/Sphereserver/Source-Archive) | Partial 0.99f source code |
| [JakubLinhart/Sphere99-56](https://github.com/JakubLinhart/Sphere99-56) | Original reconstruction effort |
| [SphereServer Source-X](https://github.com/Sphereserver/Source-X) | Active 0.56d+ development (reference) |
| [SphereCommunity](https://www.sphereserver.com/) | Community forums and docs |

## License

Based on code originally by Menace Software (www.menasoft.com). See original repository for license terms.
