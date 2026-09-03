#!/bin/sh
# test-tcc.sh: Smoke test for TCC stage-1
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TCC="$ROOT/build/linux-x64/tcc-stage1/bin/tcc"

[ -x "$TCC" ] || { echo "SKIP: TCC not built"; exit 0; }

echo "TCC version: $($TCC -v 2>&1)"

# Compile and run a trivial C program
TMPDIR=$(mktemp -d)
cat > "$TMPDIR/hello.c" << 'EOF'
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv) {
    printf("TCC test: argc=%d\n", argc);
    int x = 40 + 2;
    printf("TCC test: 40+2=%d\n", x);
    return 0;
}
EOF

$TCC -o "$TMPDIR/hello" "$TMPDIR/hello.c"
OUT="$("$TMPDIR/hello")"
echo "$OUT"
echo "$OUT" | grep -q "argc=1" || { echo "FAIL: expected argc=1"; exit 1; }
echo "$OUT" | grep -q "42" || { echo "FAIL: expected 42"; exit 1; }

# Test that we don't fall back to GCC
which "$TCC" | grep -q "tcc-stage1" || { echo "FAIL: not using stage-1 TCC"; exit 1; }

rm -rf "$TMPDIR"
echo "PASS: TCC smoke test"
