# Claude Development Instructions — SphereServer 0.99 Engine

## Project Goal

Reconstruct a fully functional SphereServer 0.99 engine that can run existing 0.99
shard scripts and save data. This is the **generic engine** — shard-specific scripts,
configuration, and customizations live in separate private repositories.

## Quick Reference

```bash
cd /workspace/Sphere99-56
make              # build sphere99svr (32-bit Linux ELF)
make clean        # remove artifacts
```

Prerequisites: `gcc-multilib g++-multilib make`

## Current State

**Server starts and runs the main game loop.** All 5 phases of core infrastructure
are implemented. 9 stubs remain (Windows registry + GUI, not applicable on Linux).

Startup sequence working:
1. Parse sphere.ini — all properties applied via virtual s_PropSet dispatch
2. Open MUL files (map0, statics, tiledata, multi)
3. Load 300+ .scp script files
4. Load world save (sphereworld.scp, spherechars.scp)
5. Load accounts
6. Initialize network socket (port 2593)
7. Enter main game loop — runs stable indefinitely

## Source Directories

```
spherelib/          Base library — fully implemented (arrays, files, strings, sockets,
                    expressions, variants, script parser, threads)
SphereCommon/       UO data structures (MUL readers, crypto, regions) — mostly implemented
SphereAccount/      Account management — functional
SphereSvr/          Main server logic — loads and runs, game logic needs work
```

## Next Priority: Client Connection (Phase 7)

The server runs but UO clients cannot connect yet. Needed:

1. **UO login encryption** — CCryptBase currently passthrough; need real handshake
   - Client sends 4-byte seed, server responds with encryption keys
   - Reference: SphereServer 0.56d `src/common/CEncrypt.cpp`
   - Also: `src/graysvr/CClientLog.cpp` for login packet handling

2. **Packet parsing** — CClient needs to parse UO protocol packets
   - Login packet (0x80), character list (0xA8), play character (0x5D)
   - Reference: `src/graysvr/CClientEvent.cpp`, `CClientMsg.cpp`

3. **Game world entry** — send map, character, items to client
   - Reference: `src/graysvr/CClientMsg.cpp` for addCharToView, addItemToView

## Key 0.99-Specific Features Still Needed

| Feature | Status | Description |
|---------|--------|-------------|
| `<?...?>` escaped macros | Not started | Deferred expression evaluation in dialogs |
| `argo.` dialog API | Not started | Dialog construction (15K+ calls in Erebor scripts) |
| `var()` globals | Partial | Server-wide variables — basic CVarDefArray works |
| `safe()` | Not started | Error-safe expression wrapper |
| Right-to-left eval | Done | `<eval 3*2+1>` = 9, not 7 |
| Extended triggers | Done | @BeforeSwing, @AfterSwing, @finalBlow etc. |

## Architecture Notes

### Threading
On Linux, the server runs single-threaded (CServTask runs in main thread).
The original Win32 design used 3 threads (ServTask, MainTask, BackTask) but
this crashes under QEMU user-mode emulation. Native Linux threading can be
re-enabled later when running on real i386 or with proper synchronization.

### Virtual Dispatch
The s_PropSet/s_PropGet/s_Method chain is critical. Signatures MUST match
exactly (especially `CGVariant&` vs `const CGVariant&`) or virtual dispatch
breaks silently — the base class stub gets called instead of the derived override.

### File Paths
- Sphere.ini and spheretables.scp use Windows backslashes — must be converted
  to forward slashes on Linux
- SCPFILES= sets m_sSCPBaseDir; script paths in [RESOURCES] are relative to CWD
- AddResourceFile checks if file exists at given path before prepending base dir

### MUL Files
- Loaded via CMulInstall::OpenFile using m_sPreferPath (set by MULFILES= in sphere.ini)
- File names are lowercase in code (map0.mul) but may be mixed case on disk
- Case-insensitive filesystem handles this transparently

## Coding Rules

- Build must remain at **0 compile errors, 0 link errors** at all times
- Must compile as **32-bit** (`-m32`) — UO protocol uses 32-bit data types
- Use `-fpermissive` for old-style C++ patterns
- Reference implementations in SphereServer 0.56d source — adapt to 0.99 API
- Guard Windows-only code with `#ifdef _WIN32`, don't remove it
- Include paths are flat — use `-I` flags, not relative `../` paths
- Use exact filename case in `#include` directives and Makefile
- Do not add Co-Authored-By to git commits
- **NEVER** commit shard-specific data: scripts/, save/, accounts/, muls/, sphere.ini, *.log
