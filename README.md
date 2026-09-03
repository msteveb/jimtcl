# Jim - Vendored Programming Environment

A small, fully vendored programming environment built around Jim Tcl.
All dependencies are included in this repository. No internet access
is required to build.

## What's Here

| Component | Version | Path | Output |
|-----------|---------|------|--------|
| Jim Tcl / jimsh | 0.84 | `src/jimtcl/` | `bin/linux-x64/jimsh` |
| TinyCC | 0.9.28rc (mob) | `src/tinycc/` | build tool |
| MiniLua / Lua | 5.5.0 | `src/minilua/` | `bin/linux-x64/lua` |
| Fennel | 1.6.1 | `src/fennel/` | `bin/linux-x64/fennel` |
| Nextvi | 7.4 | `src/nextvi/` | `bin/linux-x64/vi` |
| LuaX (pure-Lua) | 2026-08-27 | `src/luax/` | loadable modules |

## Quick Start

```sh
# Build everything for Linux x86_64
./build.sh linux

# Run tests
./build.sh test

# Cross-compile for Windows (Linux -> Win64)
./build.sh windows
```

## Build Philosophy

The build uses a three-stage bootstrap:

1. **Stage 0**: Host GCC builds TCC from source (`src/tinycc/`)
2. **Stage 1**: Stage-0 TCC rebuilds TCC (self-hosting verification)
3. **Stage 2**: Stage-1 TCC builds all other components

After stage 1, GCC is no longer used. See `BOOTSTRAP.txt` for details.

## Directory Structure

```
build.sh            Build orchestrator
BOOTSTRAP.txt       Bootstrap chain explanation
BUILD-NOTES.txt     TCC issues, workarounds, Windows status
VERSIONS.txt        Exact provenance for all vendored projects
LICENSES/           License copies for vendored projects
src/
  jimtcl/           Jim Tcl source (existing)
  tinycc/           TinyCC 0.9.28rc (mob branch)
  minilua/          MiniLua - Lua 5.5 single-header
  fennel/           Fennel Lisp 1.6.1
  nextvi/           Nextvi 7.4 terminal editor
  luax/             LuaX pure-Lua modules
bin/
  linux-x64/        Built Linux binaries
  win-x64/          Built Windows binaries (cross-compiled)
build/
  host/             Stage-0 TCC (GCC -> TCC)
  linux-x64/        Linux build artifacts
  win-x64/          Windows build artifacts
build-support/      Launcher scripts and build helpers
tests/              Smoke tests
patches/            Patches for vendored projects (if any)
comms/              Agent prompts and build reports
```

## Components

### Jim Tcl (`src/jimtcl/`)
A small, embeddable Tcl interpreter. This is the original repository
content, moved from the root into `src/jimtcl/`.

### TinyCC (`src/tinycc/`)
A small C compiler that compiles C99 source quickly. Used as the
primary build tool after stage 0. Bundles Win32 headers for
cross-compilation.

### MiniLua / Lua 5.5 (`src/minilua/`)
Lua 5.5.0 as a single-header library. Compiled into a standalone
interpreter with standard libraries (math, string, table, io, os, etc.).

### Fennel (`src/fennel/`)
A Lisp-like language that compiles to Lua. Runs on our MiniLua binary.
The fennel.lua library is built from `.fnl` sources using the Lua binary.

### Nextvi (`src/nextvi/`)
A vi/vim-like terminal text editor. Builds from a single `vi.c` file.
Not available on Windows (uses POSIX terminal APIs).

### LuaX pure-Lua modules (`src/luax/`)
Selected pure-Lua modules from the LuaX project:
F, argparse, cbor, complex, fs, imath, import, json,
luax-compat, luax-debug, luax-package, luax-version,
mathx, ps, qmath, serpent, sh, strict, sys, tar, term, toml, yaml.

Excluded: LuaSocket, LPeg, native C extensions, CLI driver (luax.lua).

## Windows Cross-Compilation

TCC can cross-compile for Win64 using its bundled Win32 headers:

```sh
./build.sh windows
```

Status:
- `lua.exe`: built and verified as PE32+ (not tested - no Wine)
- `jimsh.exe`: not yet automated (needs Windows-specific config.h)
- `vi.exe`: not possible (Nextvi uses POSIX-only APIs)
- `fennel.bat`: planned

## Licenses

- Jim Tcl: BSD-2-Clause (`src/jimtcl/LICENSE`)
- TinyCC: LGPL-2.1 (`src/tinycc/COPYING`)
- Lua/MiniLua: MIT (`src/minilua/LICENSE.txt`)
- Fennel: MIT (`src/fennel/LICENSE`)
- Nextvi: GPL-3.0 (`src/nextvi/LICENSE`)
- LuaX: GPL-3.0 (`src/luax/LICENSE`)
