#!/bin/sh
# test-jimsh.sh: Smoke test for Jim Tcl
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
JIMSH="$ROOT/bin/linux-x64/jimsh"

[ -x "$JIMSH" ] || { echo "SKIP: jimsh not built"; exit 0; }

# Version check
VER="$("$JIMSH" --version 2>&1)"
echo "jimsh version: $VER"

# Basic expression
RES="$(echo 'puts [expr {2 + 2}]' | "$JIMSH")"
[ "$RES" = "4" ] || { echo "FAIL: 2+2 expected 4, got $RES"; exit 1; }

# String ops
RES="$(echo 'puts [string toupper hello]' | "$JIMSH")"
[ "$RES" = "HELLO" ] || { echo "FAIL: string toupper expected HELLO, got $RES"; exit 1; }

# Process exec
RES="$(echo 'puts [exec echo hello]' | "$JIMSH")"
[ "$RES" = "hello" ] || { echo "FAIL: exec echo expected hello, got $RES"; exit 1; }

# Regex
RES="$(echo 'puts [regexp -inline {[0-9]+} "foo42bar"]' | "$JIMSH")"
[ "$RES" = "42" ] || { echo "FAIL: regexp expected 42, got $RES"; exit 1; }

# List operations
RES="$(echo 'puts [llength {a b c d}]' | "$JIMSH")"
[ "$RES" = "4" ] || { echo "FAIL: llength expected 4, got $RES"; exit 1; }

# info version
RES="$(echo 'puts [info version]' | "$JIMSH")"
echo "  info version: $RES"

echo "PASS: jimsh smoke test"
