# Claude Development Instructions — SphereServer 0.99 Engine

## Project Goal

Reconstruct a fully functional SphereServer 0.99 engine that can run existing 0.99
shard scripts and save data. This is the **generic engine** — shard-specific scripts,
configuration, and customizations live in separate private repositories.

## Quick Reference

```bash
make              # build sphere99svr (32-bit Linux ELF)
make clean        # remove artifacts
```

Prerequisites: `gcc-multilib g++-multilib make`

## Source Directories

```
spherelib/          Base library — MOSTLY IMPLEMENTED (arrays, files, strings, points)
SphereCommon/       UO data structures (MUL readers, crypto, regions)
SphereAccount/      Account management
SphereSvr/          Main server logic
```

## Implementation Priority

329 stubs remain (functions with `throw "not implemented"`). Implement in this order:

### Phase 1 — Script Infrastructure
1. **CScript** — `ReadLine`, `FindSection`, `ReadKeyParse`, `WriteKey`
   - Reference: `SphereServer 0.56d src/common/CScript.cpp`
2. **CGVariant** — full variant data type (string/int/ref/array/UID)
   - NOTE: 0.56d has no direct equivalent; this is 0.99-specific
3. **CExpression** — `GetComplex`, `GetValue`, `GetValueRef`
   - Must support right-to-left evaluation WITHOUT operator precedence
   - Must support `<?...?>` escaped macro syntax (0.99-only feature)
4. **CVarDef / CVarDefArray** — variable storage with `FindKeyVar`, `SetKeyVar`

### Phase 2 — Resource Loading
5. **CResourceDef** — `s_LoadProps`, `s_PropGet`, `s_PropSet`
6. **CResourceLink** — script file linking, section lookup
7. **CSphereResourceMgr** — load sphere.ini, spheretables, all .scp resources

### Phase 3 — Script Execution & Triggers
8. **CScriptExecContext** — `ExecuteCommand`, `ExecuteScript`
9. **CScriptObj** — `OnTrigger`, trigger dispatch
10. **Extended combat triggers** — add trigger enums and dispatch:
    ```
    @BeforeSwing, @AfterSwing, @beforeGetSwing, @afterGetSwing,
    @beforeDoEffect, @afterDoEffect, @beforeGetEffect,
    @finalBlow, @DrinkingPotion, @playerKill, @npckill,
    @HitMiss, @HitTry, @itemDAMAGE
    ```

### Phase 4 — Networking & Crypto
11. **CCryptBase** — client login encryption (Init, Decrypt, Encrypt)
12. **CGSocket** — full socket implementation (accept, send, receive)
13. **CClient** — client connection lifecycle

### Phase 5 — World & Persistence
14. **CWorld** — save/load sphereworld.scp, spherechars.scp
15. **CSector** — sector management, item/char tracking
16. **CAccount** — account file management

## Key 0.99-Specific Features to Implement

These features exist in 0.99 but NOT in 0.56 — no reference implementation available:

| Feature | Description |
|---------|-------------|
| `<?...?>` escaped macros | Deferred expression evaluation, used in dialogs |
| `argo.` dialog API | Dialog construction: `argo.setText()`, `argo.button()`, etc. |
| `var()` globals | Server-wide global variables: `var(name,value)` / `<var(name)>` |
| `safe()` | Error-safe expression evaluation wrapper |
| `newitemsafe()` | Safe item creation (suppresses errors) |
| `contents2()` | Container content iteration |
| `argv()/argvcount` | Function argument parsing system |
| Right-to-left eval | `<eval 3*2+1>` = 9, not 7 |

## Coding Rules

- Build must remain at **0 compile errors, 0 link errors** at all times
- Must compile as **32-bit** (`-m32`) — UO protocol uses 32-bit data types
- Use `-fpermissive` for old-style C++ patterns
- Stub functions use `throw "not implemented"` — replace with real implementations
- Use SphereServer 0.56d source as reference, but **adapt to 0.99 API** (they differ!)
- Guard Windows-only code with `#ifdef _WIN32`, don't remove it
- Include paths are flat — use `-I` flags, not relative `../` paths
- Use exact filename case in `#include` directives (GCC is case-sensitive)
- Do not add Co-Authored-By to git commits
- Do not include shard-specific scripts, configuration, or save data in this repo
