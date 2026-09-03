#!/bin/sh
# build.sh - Build orchestrator for the Jim Tcl vendored environment
# Usage: ./build.sh [target]
#   ./build.sh          - build everything (linux + docs)
#   ./build.sh linux    - build linux-x64 binaries
#   ./build.sh windows  - cross-compile win-x64 binaries (requires TCC cross)
#   ./build.sh tcc      - bootstrap TCC only (stage 0 + stage 1)
#   ./build.sh clean    - remove build/ and bin/ outputs
#   ./build.sh test     - run smoke tests
#
# Constraints:
#   - Stage 0 uses host GCC to produce TCC
#   - Stage 1 uses stage-0 TCC to rebuild TCC (self-hosting check)
#   - Everything after stage 1 uses only TCC, not GCC directly

set -e

ROOT="$(cd "$(dirname "$0")" && pwd)"
SRC="$ROOT/src"
BUILD="$ROOT/build"
BIN="$ROOT/bin"

HOST_TCC="$BUILD/host/bin/tcc"
TCC1="$BUILD/linux-x64/tcc-stage1/bin/tcc"
CROSS_TCC="$BUILD/linux-x64/tcc-cross/bin/x86_64-win32-tcc"

NPROC="$(nproc 2>/dev/null || echo 4)"

log() { echo "==> $*"; }
die() { echo "ERROR: $*" >&2; exit 1; }

# ---- PHASE 4: Bootstrap TCC ----

build_tcc_stage0() {
    log "TCC stage 0: host GCC -> TCC"
    mkdir -p "$BUILD/host"
    cd "$BUILD/host"
    if [ -x "$HOST_TCC" ]; then
        log "  already built: $HOST_TCC"
        return
    fi
    "$SRC/tinycc/configure" \
        --prefix="$BUILD/host" \
        --cc=gcc 2>&1
    make -j"$NPROC"
    make install
    log "  built: $HOST_TCC ($("$HOST_TCC" -v 2>&1))"
}

build_tcc_stage1() {
    log "TCC stage 1: stage-0 TCC -> TCC (self-hosting)"
    mkdir -p "$BUILD/linux-x64/tcc-stage1"
    cd "$BUILD/linux-x64/tcc-stage1"
    if [ -x "$TCC1" ]; then
        log "  already built: $TCC1"
        return
    fi
    [ -x "$HOST_TCC" ] || die "stage-0 TCC not found, run: ./build.sh tcc first"
    CC="$HOST_TCC" "$SRC/tinycc/configure" \
        --prefix="$BUILD/linux-x64/tcc-stage1" \
        --cc="$HOST_TCC" 2>&1
    make -j"$NPROC"
    make install
    log "  built: $TCC1"
}

build_tcc_cross() {
    log "TCC cross: building win64 cross compiler"
    mkdir -p "$BUILD/linux-x64/tcc-cross"
    cd "$BUILD/linux-x64/tcc-cross"
    if [ -x "$CROSS_TCC" ]; then
        log "  already built: $CROSS_TCC"
        return
    fi
    "$SRC/tinycc/configure" \
        --prefix="$BUILD/linux-x64/tcc-cross" \
        --enable-cross 2>&1
    make -j"$NPROC" cross-x86_64-win32
    make install
    log "  built: $CROSS_TCC"
}

# ---- PHASE 5: Linux builds ----

build_jimsh_linux() {
    log "Jim Tcl (linux): building jimsh with TCC"
    mkdir -p "$BUILD/linux-x64/jimtcl"
    cd "$BUILD/linux-x64/jimtcl"
    [ -x "$TCC1" ] || die "stage-1 TCC not found"
    CC="$TCC1" "$SRC/jimtcl/configure" \
        --prefix="$BUILD/linux-x64/jimtcl" 2>&1
    make jimsh
    cp jimsh "$BIN/linux-x64/jimsh"
    log "  built: $BIN/linux-x64/jimsh ($(ls -lh "$BIN/linux-x64/jimsh" | awk '{print $5}'))"
}

build_lua_linux() {
    log "MiniLua (linux): building Lua 5.5 with TCC"
    mkdir -p "$BUILD/linux-x64/minilua"
    [ -x "$TCC1" ] || die "stage-1 TCC not found"
    "$TCC1" -I"$SRC/minilua" \
        -o "$BUILD/linux-x64/minilua/lua" \
        "$BUILD/linux-x64/minilua/main.c" \
        -lm -ldl
    cp "$BUILD/linux-x64/minilua/lua" "$BIN/linux-x64/lua"
    log "  built: $BIN/linux-x64/lua ($(ls -lh "$BIN/linux-x64/lua" | awk '{print $5}'))"
}

build_fennel_linux() {
    log "Fennel (linux): building fennel.lua from .fnl sources"
    LUA="$BIN/linux-x64/lua"
    [ -x "$LUA" ] || die "Lua binary not found, build lua first"
    # Build fennel.lua amalgamation (precompile bootstrap files first)
    cd "$SRC/fennel"
    make LUA="$LUA" fennel.lua 2>&1
    # Install wrapper script
    cp "$ROOT/build-support/fennel-launcher.lua" "$BUILD/linux-x64/minilua/"
    cp "$BIN/linux-x64/fennel" "$BIN/linux-x64/fennel" 2>/dev/null || true
    log "  built: $BIN/linux-x64/fennel"
}

build_nextvi_linux() {
    log "Nextvi (linux): building vi with TCC"
    mkdir -p "$BUILD/linux-x64/nextvi"
    [ -x "$TCC1" ] || die "stage-1 TCC not found"
    "$TCC1" \
        -D_POSIX_C_SOURCE=200809L -D_DEFAULT_SOURCE \
        -I"$SRC/nextvi" \
        -o "$BUILD/linux-x64/nextvi/vi" \
        "$SRC/nextvi/vi.c"
    cp "$BUILD/linux-x64/nextvi/vi" "$BIN/linux-x64/vi"
    log "  built: $BIN/linux-x64/vi ($(ls -lh "$BIN/linux-x64/vi" | awk '{print $5}'))"
}

# ---- PHASE 6: Windows builds ----

build_lua_windows() {
    log "MiniLua (win-x64): cross-compiling with TCC win64"
    mkdir -p "$BUILD/win-x64/minilua"
    [ -x "$CROSS_TCC" ] || die "win64 cross TCC not found"
    "$CROSS_TCC" \
        -I"$SRC/minilua" \
        -o "$BUILD/win-x64/minilua/lua.exe" \
        "$BUILD/linux-x64/minilua/main.c"
    cp "$BUILD/win-x64/minilua/lua.exe" "$BIN/win-x64/lua.exe"
    log "  built: $BIN/win-x64/lua.exe ($(ls -lh "$BIN/win-x64/lua.exe" | awk '{print $5}'))"
}

build_nextvi_windows() {
    log "Nextvi (win-x64): cross-compiling vi.exe"
    log "  NOTE: Nextvi requires poll.h and termios.h which are POSIX-only."
    log "  Windows cross-compilation of Nextvi is not supported."
    log "  See BUILD-NOTES.txt for details."
}

build_jimsh_windows() {
    log "Jim Tcl (win-x64): cross-compiling"
    log "  NOTE: Jim Tcl Windows cross-compile requires autosetup config generation."
    log "  This is not yet automated. See BUILD-NOTES.txt."
}

# ---- Main targets ----

do_tcc() {
    build_tcc_stage0
    build_tcc_stage1
}

do_linux() {
    do_tcc
    mkdir -p "$BIN/linux-x64"
    build_jimsh_linux
    build_lua_linux
    build_fennel_linux
    build_nextvi_linux
}

do_windows() {
    do_tcc
    build_tcc_cross
    mkdir -p "$BIN/win-x64"
    build_lua_windows
    build_nextvi_windows
    build_jimsh_windows
}

do_clean() {
    log "Cleaning build/ and bin/ outputs"
    rm -rf "$BUILD/host" "$BUILD/linux-x64" "$BUILD/win-x64"
    rm -f "$BIN/linux-x64"/* "$BIN/win-x64"/*
    log "Clean done"
}

do_test() {
    log "Running smoke tests"
    for t in "$ROOT/tests"/test-*.sh; do
        if [ -f "$t" ]; then
            echo "--- $t ---"
            sh "$t" && echo "PASS" || echo "FAIL"
        fi
    done
}

case "${1:-all}" in
    all)    do_linux ;;
    linux)  do_linux ;;
    windows) do_windows ;;
    tcc)    do_tcc ;;
    clean)  do_clean ;;
    test)   do_test ;;
    *)      echo "Unknown target: $1"; echo "Usage: $0 [all|linux|windows|tcc|clean|test]"; exit 1 ;;
esac

log "Done."
