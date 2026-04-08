# Claude Development Instructions — SphereServer 0.99 Engine

## Project Goal

Reconstruct a fully functional SphereServer 0.99 engine that can run existing 0.99
shard scripts and save data. This is the **generic engine** — shard-specific scripts,
configuration, and customizations live in separate private repositories.

## Quick Reference

```bash
cd /workspace/Erebor/Migrace/Sphere99-56
make              # build sphere99svr (32-bit Linux ELF)
make clean        # remove artifacts
```

Prerequisites: `gcc-multilib g++-multilib make`

## Current State

**Server is fully functional — login, character creation, and game entry all work
with a real ClassicUO client.** The server loads 304 script files and runs stable.

Startup sequence working:
1. Parse sphere.ini — all properties applied via virtual s_PropSet dispatch
2. Open MUL files (map0, statics, tiledata, multi)
3. Load 304 .scp script files — 18,498 DEFNAMEs registered
4. Load world save — 509,594 items + 22,665 chars (3,152 player chars linked to accounts)
5. Load accounts — 1,702 accounts with char UIDs
6. Initialize network socket (port 2593)
7. Enter main game loop — runs stable indefinitely

Login sequence working end-to-end:
- TCP connect → seed → Login (0x80) → ServerList (0xA8) → ServerSelect (0xA0)
- Relay (0x8C) → CharListReq (0x91) → CharList (0xA9) with real char names
- CharPlay (0x5D) → game entry (XCMD_Start, map, view, items, light, weather)
- Character creation and game entry working
- NoCrypt + 17 encrypted client key versions supported
- Huffman compression active in game mode

Walking and collision fully functional:
- Server-side collision detection (GetHeightPoint/CheckValidMove)
- Region mapplane 255 wildcard support
- DEFNAME expression evaluator (resolves MT_WALK|MT_EQUIP|... in CAN= properties)
- Walk sub-features: stealth reveal, weather transitions, teleporters, visibility updates
- Client disconnect cleanup (no more zombie socket loops)

## Source Directories

```
spherelib/          Base library — fully implemented
SphereCommon/       UO data structures — mostly implemented
SphereAccount/      Account management — functional
SphereSvr/          Main server logic — loads, accepts clients, game logic implemented
```

## Next Priority: Game Testing & Combat

The server accepts clients, enters the game, and the script engine supports
argv/argvcount, safe(), eval(), <?...?> macros, argo.dialog API, and object
reference chaining. Primary areas needing work:

1. **Combat system** — @BeforeSwing/@AfterSwing triggers defined but combat
   pipeline (damage calc, hit/miss, effects) needs testing with real scripts

2. **`src.` reference resolution** — `<src.name>`, `<src.tag(x)>` need the same
   object chaining treatment as `<argo.tag(x)>` (src = source player/console)

3. **`var()` globals** — basic CVarDefArray works but complex var() patterns
   (var.name, var.name=value) may need additional dispatch

4. **CResourceLock** — may leak file handles (opens script files for lazy loading)

## Key 0.99-Specific Features

| Feature | Status | Description |
|---------|--------|-------------|
| Login protocol | Done | Full login → charlist → game entry works |
| CharDef/ItemDef loading | Done | 304 script files loaded, DEFNAMEs registered |
| Character creation | Done | New character creation and game entry working |
| World load | Done | 509K items + 22K chars loaded from .scp save files |
| UID system | Done | `SetUIDIndex()` fix ensures objects survive load and CharFind works |
| Walk/move | Done | Server-side collision, stealth, weather, teleporters, visibility |
| DEFNAME evaluator | Done | Expression resolver for CAN=MT_WALK\|MT_EQUIP etc. |
| World view | Done | addPlayerSee, addItem_OnGround, addChar all implemented |
| World save | Done | SaveStage/SaveForce write items/chars/accounts |
| Function dispatch tables | Done | CSCRIPT_PROPX_IMP fix — Eval, Safe, ArgV etc. resolve |
| `argv()/argvcount` | Done | Function argument access (4,377 uses in Erebor scripts) |
| `<?...?>` escaped macros | Done | Alternative expression delimiters for nesting |
| `argo.` dialog API | Done | Dialog construction with gump accumulation |
| Object ref chaining | Done | `<argo.tag(name)>`, `<argo.uid>` etc. resolve |
| `var()` globals | Partial | Server-wide variables — basic CVarDefArray works |
| `safe()` | Done | Error-safe expression wrapper |
| `eval()` | Done | Numeric expression evaluation |
| Right-to-left eval | Done | `<eval 3*2+1>` = 9, not 7 |
| Extended triggers | Done | @BeforeSwing, @AfterSwing, @finalBlow etc. |

## Architecture Notes

### Threading
On Linux, the server runs single-threaded (CServTask runs in main thread).
The original Win32 design used 3 threads (ServTask, MainTask, BackTask) but
this crashes under QEMU user-mode emulation. Native Linux threading can be
re-enabled later when running on real i386 or with proper synchronization.

### UID System
`CResourceObj::m_dwHashIndex` is the UID storage for all game objects. It must be
set explicitly after `AllocUID`/`LoadUID` — these functions store the object pointer
in the UID table but do NOT call `SetUIDIndex()`. Without `SetUIDIndex()`:
- `IsValidUID()` returns false → `IsWeird()` returns 0x3104 → object is deleted
- `FreeUID()` does nothing (m_dwHashIndex=0 → index 0, slot is empty)
- `CharFind`/`ItemFind` return NULL (UID table has dangling pointer)

Key fix in `SphereSvr/CObjBase.cpp`:
- P_Serial handler (loading): call `SetUIDIndex(dwUID)` after `LoadUID()` succeeds
- Constructor (runtime): save `AllocUID()` return value, call `SetUIDIndex(result|flags)`

Item UIDs have `UID_F_ITEM = 0x40000000` set; char UIDs have no flags.

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

## Debug Logging & Testing

### Logging (spherelog.h)
- **NEVER remove** `SPHERE_LOG_NET`/`SPHERE_LOG_ERR` messages — they are controlled by DEBUGLEVEL
- `DEBUGLEVEL=0` (sphere.ini) = errors only (production)
- `DEBUGLEVEL=2` = network trace (login, packets, relay) — default for dev
- `DEBUGLEVEL=3` = full trace (script execution)
- Use `SPHERE_LOG_NET()` for all network/client events
- Use `fprintf(stderr, "[NET]...")` for temporary pinpoint debugging (remove after fix)

### Test Tools (tools/)
- `tools/uo_test_client.py` — single login flow test (needs Huffman decompress)
- `tools/test_suite.py` — 9-test quality suite (9/9 passing)
- Run `python3 tools/test_suite.py` after every significant code change
- Add new test for each milestone (char create, game entry, walk, combat)
- After build: always `cp sphere99svr /workspace/Erebor/sphere/` then restart

### Notable Bug Fixes
- **CSCRIPT_PROPX_IMP was empty**: The `CSCRIPT_PROPX_IMP` macro was defined as empty,
  meaning ALL function dispatch tables (Eval, Safe, ArgV, Serv, etc.) had NULL entries
  and never matched. Fixed to `CScriptPropX(#a, b, c),` which populates the tables.
  This was the root cause of function dispatch being completely non-functional.
- **IsValidRID fix for single-instance resource types**: Resource types that have only
  a single instance (e.g., WEBPAGE) need special handling in IsValidRID — the index
  can be 0 and still be valid.
- **Str_Parse stub fix**: Str_Parse was stubbed out incorrectly, causing script argument
  parsing failures. Proper implementation splits strings on the given separator.
- **Password quote-stripping in accounts**: Account passwords stored with surrounding
  quotes (e.g., `"password"`) must have quotes stripped during login comparison,
  otherwise authentication fails for accounts saved in quoted format.
