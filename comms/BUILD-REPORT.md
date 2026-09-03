# Build Report

Generated: 2026-09-03
Agent: Claude Sonnet 4.6

## Summary

Successfully restructured the Jim Tcl repository into a vendored monorepo
and built all Linux x86_64 binaries using a TCC bootstrap chain.

---

## Vendored Revisions

| Project | Version | Commit SHA | Date | Source |
|---------|---------|-----------|------|--------|
| Jim Tcl | 0.84 (fork) | 825be07 (base) | existing | this repo |
| TinyCC | 0.9.28rc | 2ba12e83b3599ca8f5d50c179fe5138fe956f0c9 | 2026-08-09 | github.com/TinyCC/tinycc mob |
| MiniLua | Lua 5.5.0 | c326bae1f2293a1ae20648669bed1a8f4585cccc | 2026-01-05 | github.com/edubart/minilua |
| Fennel | 1.6.1 | 9af5e47fa7ddea806544d15507412ddc51bd046d | 2025-12-30 | github.com/bakpakin/Fennel tag 1.6.1 |
| Nextvi | 7.4 | 4f5c2d4d3b1542ec33dcf7248af4cbe354bf7830 | 2026-08-29 | github.com/kyx0r/nextvi tag 7.4 |
| LuaX (pure-Lua) | 2026-08-27 | 9870d9aaa7b766a1c6b61738f1d7cc99e0bbc77e | 2026-08-27 | codeberg.org/cdsoft/luax |

---

## Directory Tree (top-level)

```
/workspaces/jim/
  README.md           Project overview
  VERSIONS.txt        Exact provenance for all vendored projects
  BOOTSTRAP.txt       Stage 0/1/2 bootstrap explanation
  BUILD-NOTES.txt     TCC issues, workarounds, known problems
  build.sh            Build orchestrator (orchestrates all phases)
  comms/              Agent communications
    AGENT-PROMPT.md   Original task prompt
    BUILD-REPORT.md   This file
  LICENSES/           (directory for license copies - use src/*/LICENSE)
  patches/            (empty - no patches needed yet)
  src/
    jimtcl/           Jim Tcl source (312 files, moved from root)
    tinycc/           TinyCC 0.9.28rc mob branch
    minilua/          MiniLua (Lua 5.5 single header)
    fennel/           Fennel 1.6.1 (full source + compiled fennel.lua)
    nextvi/           Nextvi 7.4
    luax/             24 pure-Lua LuaX modules
  bin/
    linux-x64/        jimsh, lua, fennel (script), vi
    win-x64/          lua.exe (cross-compiled)
  build/
    host/             Stage-0 TCC (GCC -> TCC)
    linux-x64/
      tcc-stage1/     Stage-1 TCC (TCC -> TCC, self-hosted)
      tcc-cross/      Win64 cross TCC (x86_64-win32-tcc)
      jimtcl/         Jim Tcl build artifacts
      minilua/        MiniLua build artifacts + main.c
      nextvi/         Nextvi build artifacts
    win-x64/
      minilua/        Windows lua.exe build artifacts
  tests/
    test-tcc.sh       TCC smoke test
    test-lua.sh       Lua 5.5 smoke test
    test-fennel.sh    Fennel 1.6.1 smoke test
    test-jimsh.sh     Jim Tcl smoke test
    test-nextvi.sh    Nextvi smoke test
    test-luax.sh      LuaX modules smoke test
  build-support/
    fennel-launcher.lua  Fennel CLI launcher (used by bin/linux-x64/fennel)
```

---

## Binaries Produced

### Linux x86_64 (run-tested)

| Binary | Size | Method | Test Status |
|--------|------|--------|-------------|
| bin/linux-x64/jimsh | 519,068 bytes | TCC stage-1 | PASS - version, exec, regexp, list |
| bin/linux-x64/lua | 393,796 bytes | TCC stage-1 | PASS - _VERSION, math, string, stdlib |
| bin/linux-x64/fennel | 130 bytes (script) | shell wrapper | PASS - version, eval, let, fn |
| bin/linux-x64/vi | 303,436 bytes | TCC stage-1 | PASS - version string (no tty for interactive) |
| src/fennel/fennel.lua | 301,522 bytes | compiled from .fnl | used by fennel script |

### Windows x86_64 (compile-tested only, no Wine)

| Binary | Size | Method | Test Status |
|--------|------|--------|-------------|
| bin/win-x64/lua.exe | 369,664 bytes | TCC x86_64-win32 | compile-tested, file type verified as PE32+ |

---

## Smoke Test Results (./build.sh test)

All tests PASS:
- test-tcc.sh:     TCC stage-1 compiles and runs trivial C (argc, arithmetic)
- test-lua.sh:     Lua 5.5 - _VERSION, math, string, table, arg table
- test-fennel.sh:  Fennel 1.6.1 - version, eval, let binding, fn
- test-jimsh.sh:   Jim Tcl 0.84 - version, exec, regexp, list ops
- test-nextvi.sh:  Nextvi 7.4 - version string in usage (no tty for interactive)
- test-luax.sh:    LuaX - strict, serpent, json, F modules load and run

---

## Patches Applied

None. All vendored projects built cleanly from their tagged/committed sources.

The only modifications to vendored sources:
- src/fennel/fennel.lua: generated (built from .fnl sources using our lua binary)
- src/fennel/bootstrap/macros.lua: generated (same)
- src/fennel/bootstrap/match.lua: generated (same)
- src/fennel/bootstrap/view.lua: generated (same)

---

## Bootstrap Chain

```
Host GCC 13.3
  -> build/host/bin/tcc (stage 0, v0.9.28rc)
     -> build/linux-x64/tcc-stage1/bin/tcc (stage 1, self-hosted)
        -> bin/linux-x64/jimsh (Jim Tcl 0.84)
        -> bin/linux-x64/lua   (Lua 5.5.0 via MiniLua)
        -> bin/linux-x64/vi    (Nextvi 7.4)
     -> build/linux-x64/tcc-cross/bin/x86_64-win32-tcc (Win64 cross)
        -> bin/win-x64/lua.exe (PE32+ Lua 5.5)
  
  src/fennel/*.fnl --[lua]--> src/fennel/fennel.lua (library)
  build-support/fennel-launcher.lua + bin/linux-x64/lua = bin/linux-x64/fennel
```

---

## Unresolved Windows Issues

1. **Nextvi for Windows**: Not possible without porting. Requires poll.h,
   termios.h, sys/ioctl.h - all POSIX-only. No Windows port exists.

2. **Jim Tcl for Windows**: Needs Windows-specific jim-config.h generated
   by autosetup. Not automated. Would require running autosetup with a
   Windows target spec or hand-crafting the config header.

3. **No Wine available**: Cannot run .exe files to verify they work.
   lua.exe is verified as a valid PE32+ by `file` command.

4. **Fennel standalone binary**: The fennel --compile-binary feature would
   produce a self-contained fennel.exe but requires linking against Lua's
   C library, which needs more work in the Windows cross-compile chain.

---

## Known Issues / BUILD-NOTES Summary

1. TCC stage-1 requires stage-0 to be *installed* (not just built) to find
   its own include files when used as CC. Fixed by running `make install`
   before using as CC.

2. MiniLua main.c needed to populate the `arg` global for script-mode Lua.
   Without it, Fennel's aot.lua fails. Fixed in the custom main.c.

3. Fennel's standalone launcher (binary.fnl) fails to compile because the
   bootstrap compiler (v0.4.4-dev) is missing the `view` function used
   in macrodebug. Worked around with a shell script launcher.

4. jim-mk.cpp (Metakit) requires g++/clang++ and is excluded from TCC builds.

See BUILD-NOTES.txt for full details.

---

## Next Steps

1. Automate Jim Tcl Windows cross-compile (hand-craft Windows jim-config.h)
2. Add LuaX native C modules once C compilation path is stable
3. Add Wine to CI environment for Windows exe testing
4. Add more comprehensive tests for each component
5. Consider adding a fennel.exe build (link lua.a + fennel.lua together)
6. Add version pinning / update script for when upstream versions change
