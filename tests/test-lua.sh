#!/bin/sh
# test-lua.sh: Smoke test for MiniLua (Lua 5.5)
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LUA="$ROOT/bin/linux-x64/lua"

[ -x "$LUA" ] || { echo "SKIP: Lua not built"; exit 0; }

# Version check
VER="$("$LUA" -e "print(_VERSION)")"
echo "Lua version: $VER"
echo "$VER" | grep -q "Lua 5" || { echo "FAIL: expected Lua 5.x"; exit 1; }

# Basic math
RES="$("$LUA" -e "print(2+2)")"
[ "$RES" = "4" ] || { echo "FAIL: 2+2 expected 4, got $RES"; exit 1; }

# String ops
RES="$("$LUA" -e "print(string.upper('hello'))")"
[ "$RES" = "HELLO" ] || { echo "FAIL: string.upper expected HELLO, got $RES"; exit 1; }

# Table/stdlib
RES="$("$LUA" -e "local t={10,20,30}; print(#t)")"
[ "$RES" = "3" ] || { echo "FAIL: #t expected 3, got $RES"; exit 1; }

# Math lib
RES="$("$LUA" -e "print(math.floor(3.7))")"
[ "$RES" = "3" ] || { echo "FAIL: math.floor expected 3, got $RES"; exit 1; }

# arg table
RES="$("$LUA" -e "print(type(arg))")"
[ "$RES" = "table" ] || { echo "FAIL: arg should be table, got $RES"; exit 1; }

echo "PASS: Lua smoke test"
