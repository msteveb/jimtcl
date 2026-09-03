# Agent Task Prompt

This file contains the original task prompt given to the Claude Code agent
for this repository restructuring and vendoring task.

---

You are performing a large repository restructuring and vendoring task.
Work in /workspaces/jim (a GitHub Codespace with a Jim Tcl fork). Read
the full task carefully and execute it step by step.

## OVERVIEW

Transform this Jim Tcl repository into a small, fully vendored programming
environment with:
1. Jim Tcl / jimsh - interactive shell
2. TinyCC - small native C compiler (bootstrap root)
3. MiniLua/Lua + Fennel - scripting VM + Lisp-like language
4. Nextvi - terminal text editor

[Full prompt was approximately 200 lines covering phases 0-11 of the task]

## Key Phases Executed

- Phase 0: Inspect current state
- Phase 1: Create directory structure
- Phase 2: Move Jim Tcl source to src/jimtcl/ using git mv
- Phase 3: Vendor TinyCC, MiniLua, Fennel, Nextvi, LuaX
- Phase 4: Bootstrap TCC (stage 0: GCC->TCC, stage 1: TCC->TCC)
- Phase 5: Build Linux components with TCC (jimsh, lua, fennel, vi)
- Phase 6: Windows cross-compilation investigation (lua.exe built)
- Phase 7: LuaX integration (pure-Lua modules vendored)
- Phase 8: Build script (build.sh)
- Phase 9: Smoke tests (tests/test-*.sh)
- Phase 10: Documentation (README.md, BOOTSTRAP.txt, VERSIONS.txt, BUILD-NOTES.txt)
- Phase 11: Comms folder (this file + BUILD-REPORT.md)

## Constraints (from original prompt)

- NO network access during build (only during vendoring phase)
- NO git submodules
- NO silently falling back to GCC after stage 0
- NO cmake/meson/ninja as required deps
- DO document everything that fails instead of hiding it
- DO remove nested .git/ dirs from vendored projects before committing
- DO preserve all upstream licenses
