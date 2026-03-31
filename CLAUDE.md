# Claude Development Instructions

## Project Goal

Reconstruct SphereServer 0.99 source code to run the Erebor UO shard (uoerebor.cz).
The shard runs on a **custom fork** of 0.99z8 with proprietary combat triggers.

## Quick Reference

```bash
make              # build sphere99svr (32-bit Linux ELF)
make clean        # remove artifacts
```

Prerequisites: `gcc-multilib g++-multilib make`

## Repository Layout

- `master` — stable base, Linux build that compiles and links
- `variant-a/sphere99-reconstruction` — PRIMARY: complete 0.99 stubs for functional server
- `variant-b/migrate-to-056d` — ON HOLD: migration deemed infeasible (510K lines of incompatible scripts)
- All branches protected — changes go through PR with review (owner MyKEms can bypass)

## Source Directories

```
spherelib/          Base library — MOSTLY IMPLEMENTED (arrays, files, strings, points)
SphereCommon/       UO data structures (MUL readers, crypto, regions)
SphereAccount/      Account management
SphereSvr/          Main server logic
```

## Related Resources

| Path / URL | What |
|-----------|------|
| `/workspace/erebor-sphere/` | Erebor shard (scripts + save + sphere.ini) from gitlab.com/erebor/sphere |
| `/workspace/sphere99zl/` | Original 0.99zl distribution (binary + default scripts) |
| `/workspace/SphereServer99/` | SphereServer 0.56d (fully builds, reference for stub implementations) |

## Erebor Shard — Critical Details

### Scale
- **510,000 lines** across 526 .scp files (Czech language)
- 14,760 ITEMDEFs, 1,517 CHARDEFs, 3,580 functions, 758 dialogs, 383 event blocks
- Production save data in `/workspace/erebor-sphere/save/`
- Server version in save: `Version="0.99z8"`, `SaveCount=13300`

### Custom SphereSvr.exe Fork
The Erebor binary is NOT a standard 0.99z8. It has **custom triggers compiled in**:

```
@BeforeSwing     @AfterSwing       @beforeGetSwing    @afterGetSwing
@beforeDoEffect  @afterDoEffect    @beforeGetEffect
@finalBlow       @DrinkingPotion   @playerKill        @npckill
@HitMiss         @HitTry           @itemDAMAGE
```

These must be added to our trigger dispatch in `SphereSvr/` source code.

### Guardian NPC System
An invisible immortal NPC (`c_nastaveni_guardian`) that:
- Fires a timer every 10 seconds to apply configuration changes
- Teleports to player's position on login (wakes up sectors)
- Serves as item factory via `newitemsafe()`
- UID stored in `var(nastaveni_guardian_uid)` global
- Referenced from: `nastaveni.scp`, `sphereevents.scp`, `funkce.scp`, `dobyvani-declarator.scp`, `nearbySearch.scp`

### 0.99-Specific Features Used Extensively
| Feature | Count | Description |
|---------|-------|-------------|
| `<?...?>` escaped macros | 2,107 | Deferred expression evaluation, 0.99-only |
| `argo.` dialog API | 15,000+ | `setText`, `newTextLine`, `button`, `dialog_textpos` |
| `argv()/argvcount` | 4,377 | Function argument parsing |
| `var()` globals | 149 | Server-wide global variables |
| `safe()` | 654 | Error-safe expression evaluation |
| `serv.time` | 704 | Server tick time |
| `src.qtag()` | 2,832 | Quest tag system |
| `mapplane` | 319 | Multi-map support |
| `sector.allchars/items()` | 88 | Sector iteration |
| `profession` | 469 | Class system property |
| `newitemsafe()` | 109 | Safe item creation via guardian |

## Implementation Priority (Variant A)

329 stubs remain. Implement in this order:

### Phase 1 — Script Infrastructure (must work first)
1. **CScript** — `ReadLine`, `FindSection`, `ReadKeyParse`, `WriteKey`
   - Reference: `SphereServer99/src/common/CScript.cpp`
2. **CGVariant** — full variant data type (string/int/ref/array/UID)
   - NOTE: 0.56d has no direct equivalent; this is 0.99-specific
3. **CExpression** — `GetComplex`, `GetValue`, `GetValueRef`
   - Must support right-to-left evaluation WITHOUT operator precedence
   - Must support `<?...?>` escaped macro syntax
   - Reference: `SphereServer99/src/common/CExpression.cpp` (adapt!)
4. **CVarDef / CVarDefArray** — variable storage with `FindKeyVar`, `SetKeyVar`

### Phase 2 — Resource Loading
5. **CResourceDef** — `s_LoadProps`, `s_PropGet`, `s_PropSet`
6. **CResourceLink** — script file linking, section lookup
7. **CSphereResourceMgr** — load sphere.ini, spheretables, all .scp resources
   - Reference: `SphereServer99/src/graysvr/CResource.cpp`

### Phase 3 — Script Execution
8. **CScriptExecContext** — `ExecuteCommand`, `ExecuteScript`
9. **CScriptObj** — `OnTrigger`, trigger dispatch
10. **Custom Erebor triggers** — add trigger enums and dispatch for all 14+ custom triggers

### Phase 4 — Networking & Crypto
11. **CCryptBase** — client login encryption (Init, Decrypt, Encrypt)
12. **CGSocket** — full socket implementation (accept, send, receive)
13. **CClient** — client connection lifecycle

### Phase 5 — World & Persistence
14. **CWorld** — save/load `sphereworld.scp`, `spherechars.scp`
15. **CSector** — sector management, item/char tracking
16. **CAccount** — account file management

## Why NOT 0.56d Migration

Analysis of Erebor scripts confirmed migration is infeasible:
- `<?...?>` syntax (2,107 uses) has NO 0.56d equivalent
- `argo.` dialog API (15,000+ calls) is completely different in 0.56d
- Custom combat triggers don't exist in 0.56d
- `var()` globals work differently
- Expression evaluation semantics differ (right-to-left vs standard)
- Community consensus: "practically impossible" for large shards
- Estimated effort: rewrite 300K+ lines of scripts

## Coding Rules

- Build must remain at **0 compile errors, 0 link errors** at all times
- Must compile as **32-bit** (`-m32`)
- Use `-fpermissive` for old-style C++ patterns
- Stub functions use `throw "not implemented"` — replace with real code
- Reference implementations in `/workspace/SphereServer99/src/` (adapt to 0.99 API!)
- Guard Windows-only code with `#ifdef _WIN32`, don't remove it
- Include paths are flat — use `-I` flags, not relative `../` paths
- Use exact filename case in `#include` directives
- Do not add `Co-Authored-By` to git commits
