#!/bin/sh
# test-fennel.sh: Smoke test for Fennel 1.6.1
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
FENNEL="$ROOT/bin/linux-x64/fennel"
LUA="$ROOT/bin/linux-x64/lua"

[ -x "$FENNEL" ] || { echo "SKIP: Fennel not built"; exit 0; }
[ -x "$LUA" ] || { echo "SKIP: Lua dependency not found"; exit 0; }

# Version check
VER="$("$FENNEL" --version 2>&1)"
echo "Fennel version: $VER"
echo "$VER" | grep -q "1.6" || { echo "FAIL: expected Fennel 1.6.x"; exit 1; }

# Basic eval
RES="$("$FENNEL" -e "(print (+ 1 2 3))")"
[ "$RES" = "6" ] || { echo "FAIL: (+ 1 2 3) expected 6, got $RES"; exit 1; }

# String manipulation
RES="$("$FENNEL" -e "(print (string.upper \"hello\"))")"
[ "$RES" = "HELLO" ] || { echo "FAIL: string.upper expected HELLO, got $RES"; exit 1; }

# Fennel let binding
RES="$("$FENNEL" -e "(let [x 10 y 20] (print (+ x y)))")"
[ "$RES" = "30" ] || { echo "FAIL: let binding expected 30, got $RES"; exit 1; }

# Compile a Fennel form
RES="$("$FENNEL" -e "(fn add [a b] (+ a b)) (print (add 7 8))")"
[ "$RES" = "15" ] || { echo "FAIL: fn expected 15, got $RES"; exit 1; }

echo "PASS: Fennel smoke test"
