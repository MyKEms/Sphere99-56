# Sphere99-56 — SphereServer 0.99 Reconstruction

Reconstructed source code for **SphereServer 0.99** — a game server emulator for
Ultima Online 2D, originally developed by Menasoft (Dennis Robinson). The 0.99 branch
was proprietary and never officially open-sourced. This project aims to reconstruct
a fully functional 0.99-compatible server from available partial sources.

## Current Status

**The server loads the world, accepts UO client connections, and runs the game loop.**

| Metric | Value |
|--------|-------|
| Compile errors | **0** |
| Link errors | **0** |
| Remaining stubs | **9** (Windows-only: registry + GUI, N/A on Linux) |
| Binary | `sphere99svr` — ~2.0 MB, 32-bit ELF (Linux x86) |
| Source files | 148 (.cpp + .h), ~101,500 lines |
| World load | 509K items, 22K chars, 3K player chars linked to 1.7K accounts |
| Network | Login → ServerList → CharList → Game entry works |
| Encryption | NoCrypt + 17 client key versions (XOR rotation) |

## Building

```bash
# Prerequisites (Ubuntu/Debian)
sudo apt-get install -y gcc-multilib g++-multilib make

# Build
make            # produces sphere99svr (32-bit ELF binary)
make clean      # removes build artifacts
```

## Running

The server needs UO data files and scripts to run (not included in this repo):

```bash
# Directory structure expected:
your-shard/
├── sphere99svr          # compiled binary
├── sphere.ini           # server configuration
├── spheretables.scp     # script loading order
├── muls/                # UO MUL data files (map0.mul, tiledata.mul, etc.)
├── scripts/             # .scp script files
├── save/                # world save data
└── accounts/            # player accounts

cd your-shard && ./sphere99svr
# Output: "Press '?' for console commands"
```

Note: paths in sphere.ini and spheretables.scp must use forward slashes (`/`) on Linux.

## Background

SphereServer had two parallel development branches that are **not sequential versions**:

```
1998  GrayWorld (Dennis Robinson / Menasoft)
        │
        ├─→ 0.55 → 0.56a → 0.56b → 0.56d    (community, open-source)
        │
        └─→ 0.99a → 0.99f → 0.99z8 → 0.99zl  (Menasoft, proprietary, closed)
```

Scripts between 0.99 and 0.56 are **fundamentally incompatible** — different expression
syntax (`<?...?>` vs `<...>`), trigger systems, dialog APIs, and variable mechanisms.

This project reconstructs the 0.99 source from:
- Partial 0.99f source from [Sphereserver/Source-Archive](https://github.com/Sphereserver/Source-Archive)
- Gap-filling with compatible code from [JakubLinhart/Sphere99-56](https://github.com/JakubLinhart/Sphere99-56)
- Linux/GCC port and implementations (this fork)

## Project Structure

```
spherelib/          Base library (strings, files, arrays, sockets, threads, expressions)
SphereCommon/       Shared UO data structures (maps, tiles, crypto, regions)
SphereAccount/      Account management
SphereSvr/          Main server (game logic: characters, items, clients, world)
Makefile            GNU Make build system (Linux/GCC)
CLAUDE.md           Technical documentation and development instructions
```

## Development Roadmap

### Phase 1 — Script Infrastructure
- [x] CScript parser (ReadLine, FindSection, ReadKeyParse, WriteKey)
- [x] CGVariant (tagged union: string/int/DWORD/UID/ref/array)
- [x] CExpression evaluator (right-to-left, no precedence — 0.99 behavior)
- [x] CVarDefArray (key-value variable storage with CVarDefStr/CVarDefNum)
- [x] CAtomRef (reference-counted atom strings)
- [x] Str_Parse, Str_Match, Str_ParseCmds and other string utilities

### Phase 2 — Resource Loading
- [x] CLog (configurable level and group mask logging)
- [x] CResourceDef / CResourceLink / CResourceScript
- [x] CResourceMgr (AddResourceFile, LoadResources, OpenScriptFind, AddResourceDir)
- [x] CGFile helpers (ExtractPath, GetFileNameTitle, GetFileNameExt)
- [x] sphere.ini property dispatch (s_PropSet virtual chain fix)
- [x] Script path resolution (SCPFILES base dir, backslash normalization)

### Phase 3 — Script Execution & Triggers
- [x] CScriptExecContext (ExecuteScript with IF/ELSE/WHILE/FOR/RETURN control flow)
- [x] CResourceLock (open resource sections for reading)
- [x] CResourceTriggered::OnTriggerScript (trigger dispatch)
- [x] Table lookup functions (FindTable, FindTableHead, s_FindKeyInTable)
- [x] CScriptPropArray::AddProps (merged function tables)
- [x] 11 extended combat triggers (@BeforeSwing, @AfterSwing, @finalBlow, etc.)

### Phase 4 — Networking & Crypto
- [x] CGSocket (full POSIX TCP socket: create, bind, listen, accept, send/recv)
- [x] CSocketAddress / CSocketNamedAddr (address parsing, DNS resolution)
- [x] CGSocketSet (fd_set wrapper for select() multiplexing)
- [x] CLogIP / CLogIPArray (connection tracking, flood protection)
- [x] CCryptBase (passthrough crypto — real UO encryption TBD)
- [x] Real UO client encryption (17 key versions, XOR rotation cipher)
- [x] Huffman compression for game-mode packets

### Phase 5 — World Persistence
- [x] CVarDefArray tag persistence (s_PropSetTags, s_WriteTags)
- [x] s_FixExtendedProp (compound property keys: Tag.xyz, Attr_xxx)
- [x] String utilities (Str_ahextou, Str_GetBare, Str_Match, etc.)
- [x] Unicode conversion (CvtUNICODEToSystem, CvtSystemToUNICODE)
- [x] World save loading (sphereworld.scp, spherechars.scp)
- [x] World save writing (SaveStage/SaveForce — staged sector-by-sector save)

### Phase 6 — Server Stability & Runtime
- [x] Fix static init crash (custom operator new/delete with malloc/free)
- [x] Fix AddSortKey argument order (21 call sites)
- [x] Fix FOR_HASH macro (proper iteration)
- [x] Fix s_PropSet vtable dispatch (const/non-const signature mismatch)
- [x] Fix script path double-prefix (scripts/scripts/ → scripts/)
- [x] Single-threaded mode on Linux (avoids QEMU threading issues)
- [x] CServTimeMaster first-tick time delta fix
- [x] Fix UID system — SetUIDIndex after AllocUID/LoadUID (objects survive loading)
- [x] Fix hex number parsing (strtol base 16 for 0-prefixed values)
- [x] SIGSEGV crash handler with backtrace
- [ ] Multi-threaded mode (native Linux, not under QEMU)

### Phase 7 — Client Connection
- [x] UO client login encryption (seed, keys, handshake)
- [x] Login → ServerList → ServerSelect → Relay flow
- [x] Character list with real char names from world save
- [x] Character selection → game world entry
- [x] Game entry packets (XCMD_Start, view, items, light, weather)
- [x] Walk/movement event handling
- [x] Item display, click, pickup, drop, equip packets

### Phase 8 — Game Logic & Script Support (current)
- [ ] `<?...?>` escaped macro evaluation (0.99-specific)
- [ ] `argo.` dialog construction API (15K+ calls in Erebor scripts)
- [ ] Full `var()` global variable system
- [ ] `safe()` error-safe expression wrapper
- [ ] Complete trigger dispatch for all game events
- [ ] NPC AI and pathfinding
- [ ] Combat calculations
- [ ] Skill and spell systems

## Contributing

Contributions welcome! All changes go through Pull Requests with code review.

When implementing features, use [SphereServer 0.56d source](https://github.com/SphereServer/Source)
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
