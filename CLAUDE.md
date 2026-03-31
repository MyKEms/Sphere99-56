# Sphere99-56 — Linux Build Guide

## What is this?

Reconstructed source code for **SphereServer 0.99zg** — a game server emulator for
Ultima Online 2D. Originally developed by Menasoft (Dennis Robinson), the 0.99 branch
was proprietary and never officially open-sourced.

This repo (`JakubLinhart/Sphere99-56`) fills gaps in the partial 0.99 source code
using code from the open-source 0.56 branch. We've ported it from Win32/MSVC to
Linux/GCC.

### Version info

| Item | Version |
|------|---------|
| Source code base (this repo) | **0.99u** (SPHERE_VERSION in spherecommon.h) |
| Repo targets reimplementation of | **0.99zg** |
| Original binary we want to match | **0.99zl** (Nov 30, 2003) |
| Gap: 0.99zg → 0.99zl | ~5 minor versions (zh, zi, zj, zk, zl) |

### Related resources

| Path | Description |
|------|-------------|
| `/workspace/sphere99zl/` | Original 0.99zl distribution (EXE + scripts) |
| `/workspace/sphere99zl/svr/SphereSvr.exe` | Original Win32 binary (1.05 MB, MSVC 7.1, Dec 2003) |
| `/workspace/sphere99zl/svr/SphereSvr_REVISIONS.txt` | Changelog from 99w5 through 99zl |
| `/workspace/sphere99zl/scripts/` | Complete script pack (.scp files) |
| `/workspace/SphereServer99/` | SphereServer 0.56d fork (fully functional on Linux, reference implementation) |

## Building

### Prerequisites

```bash
sudo apt-get install -y gcc-multilib g++-multilib make
```

### Build

```bash
cd /workspace/Sphere99-56
make            # builds sphere99svr (32-bit ELF)
make clean      # remove all .o files and binary
```

### Output

- Binary: `sphere99svr` (~1.9 MB, 32-bit x86 ELF)
- Compiler: g++ 11.4, C++14, `-m32 -fpermissive`
- Defines: `-DSPHERE_SVR -D_CONSOLE -D_MT`

## Project structure

```
Sphere99-56/
├── spherelib/          # Base library (strings, files, arrays, sockets, threads)
│   ├── common.h        # Linux type defs (DWORD, BYTE, HANDLE, HRESULT macros)
│   ├── CArray.h        # CGTypedArray, CGObList, CGRefArray (IMPLEMENTED)
│   ├── cfile.h/cpp     # CFile, CFileText, CGFile (IMPLEMENTED - POSIX)
│   ├── cstring.h/cpp   # CGString
│   ├── CPointBase.cpp  # CGPointBase geometry (IMPLEMENTED)
│   ├── CSocket.h       # Network sockets (Linux headers added)
│   ├── CThread.h       # Threading primitives
│   ├── CExpression.h   # CGVariant, CVarDef, expression evaluation
│   ├── CScript.h       # Script file parser
│   └── stubs.cpp       # Stub implementations for linking
├── SphereCommon/       # Shared UO data structures
│   ├── spherecommon.h  # Master include, version string
│   ├── ccrypt.h/cpp    # Client encryption
│   ├── cMul*.h/cpp     # MUL file readers (maps, tiles, multis)
│   ├── cSphereExp.h/cpp # Script expression context
│   ├── cresourcebase.h # Resource definition base classes
│   ├── cregion*.h/cpp  # Region/area system
│   └── stubs.cpp       # Stub implementations
├── SphereAccount/      # Account management
│   ├── caccount.h/cpp  # CAccount class
│   └── caccountmgr.cpp # Account manager
├── SphereSvr/          # Main server (game logic)
│   ├── spheresvr.h     # Master server header
│   ├── CChar*.cpp      # Character logic (movement, combat, skills, spells)
│   ├── CClient*.cpp    # Client connection handling
│   ├── CItem*.cpp      # Item logic
│   ├── CWorld*.cpp     # World management
│   ├── CServer.cpp     # Server initialization
│   ├── cresource*.cpp  # Resource/script loading
│   └── stubs.cpp       # Static class instances for linking
└── Makefile            # GNU Make build system
```

## Current status

### What works
- Full compilation: **0 errors**, 0 undefined references
- Links into a 32-bit Linux executable
- File I/O (CFile, CFileText, CGFile) — real POSIX implementations
- Arrays and containers (CGTypedArray, CGObList, CGRefArray) — real implementations
- Point/geometry math (CGPointBase) — real implementations
- String operations (CGString) — real implementations
- Memory management (CMemBlockBase) — real implementations

### What is stubbed (329 remaining stubs)
Most stubs are in `spherelib/` (247) — these are base library functions that the
Sphere99-56 author left as `throw "not implemented"` placeholders.

**Critical stubs that need real implementations to run:**

1. **Script parsing** — `CScript::ReadLine`, `FindSection`, `ReadKeyParse`
   - Without this, no .scp scripts can be loaded
   - Reference: `SphereServer99/src/common/CScript.cpp`

2. **Expression evaluation** — `CExpression::GetComplex`, `GetValue`
   - Script variable evaluation
   - Reference: `SphereServer99/src/common/CExpression.cpp`

3. **Resource loading** — `CResourceDef::s_LoadProps`, `s_PropGet`, `s_PropSet`
   - Loading ITEMDEF, CHARDEF, etc. from scripts
   - Reference: `SphereServer99/src/common/CResourceBase.cpp`

4. **Script execution** — `CScriptExecContext::ExecuteCommand`, `ExecuteScript`
   - Running trigger scripts
   - Reference: `SphereServer99/src/common/CScriptObj.cpp`

5. **Client crypto** — `CCryptBase::Init`, `Decrypt`, `Encrypt`
   - Client login encryption (partially stubbed)
   - Reference: `SphereServer99/src/common/CEncrypt.cpp`

6. **Variant/variable system** — `CGVariant` all methods
   - Data type used everywhere for script variables
   - Reference: no direct 0.56 equivalent (0.56 uses different approach)

7. **Network** — `CGSocket`, `CSocketAddress` full implementation
   - Accepting client connections
   - Reference: `SphereServer99/src/common/CSocket.cpp`

### Version gap: 0.99zg → 0.99zl

Features from REVISIONS.txt that are in 0.99zl but possibly missing from this source:

| Version | Key changes |
|---------|-------------|
| **99zh** | Region conflicts on map 255 fix, `can_dye=0`, item prop updates |
| **99zi** | Bag nesting rules, HTTPPORT, FEATUREMASK1/2, Sockets 2.0, tracking fix |
| **99zj** | Multithread protect atoms, SphereMaster support |
| **99zk** | Crash fixes, AllClients command fix, Secure=1 fix |
| **99zl** | IP restrict for accounts, noclose/nomove dialog flags, raceclass regen props |

## Porting notes (what was changed from upstream)

### Include system
- All `#include "..\path\file.h"` backslash paths → forward slashes
- All `#include "../relative/path/file.h"` → flat `#include "file.h"` with `-I` flags
- Case mismatches fixed (21 files) — filesystem is case-insensitive but GCC is case-sensitive for `#pragma once`
- Added `#ifndef` include guards to all spherelib headers (GCC `#pragma once` fails with different-case paths)

### Linux compatibility (spherelib/common.h)
- Windows types: BYTE, WORD, DWORD, UINT, HANDLE, HWND, BOOL, LPCSTR, LPCTSTR, etc.
- Win32 structs: POINT, POINTS, RECT
- Win32 macros: MAKEWORD, LOBYTE, HIBYTE, LOWORD, HIWORD, TEXT
- MSVC functions: `_stricmp` → `strcasecmp`, `_strupr`, `_strlwr`, `_vsnprintf`, `_snprintf`
- HRESULT system: IS_ERROR, FAILED, SUCCEEDED, E_FAIL, NO_ERROR, HRES_* codes
- `Sleep()` → `usleep()`, `GetTickCount()` → `time(NULL)` in various places

### Platform guards added
- `CSocket.h` — Linux socket headers (`<sys/socket.h>`, `<netinet/in.h>`, etc.)
- `CThread.h` — `THREAD_ENTRY_RET` for Linux
- `cwindow.h`, `cregistry.h`, `CNTService.h` — `#ifdef _WIN32` guards
- `cservconsolew.cpp` excluded from build (Windows GUI)
- `cDSound.cpp`, `cdsoundchat.cpp` excluded (DirectSound)
- `cbacktask.cpp` — `ExitThread()` → `return`

### New files created
- `spherelib/cfile.cpp` — POSIX implementations for CFile/CFileText/CGFile
- `spherelib/stubs.cpp` — Linking stubs for spherelib classes
- `SphereCommon/stubs.cpp` — Linking stubs for SphereCommon classes
- `SphereSvr/stubs.cpp` — Static class instances and stubs for SphereSvr
- `Makefile` — GNU Make build system (ported from MSVC .vcxproj)
