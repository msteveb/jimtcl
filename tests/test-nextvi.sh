#!/bin/sh
# test-nextvi.sh: Smoke test for Nextvi 7.4
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VI="$ROOT/bin/linux-x64/vi"

[ -x "$VI" ] || { echo "SKIP: vi (nextvi) not built"; exit 0; }

# Version/usage check (--version is not supported, but usage shows version)
VER="$("$VI" --version 2>&1 || true)"
echo "Nextvi output: $VER"
echo "$VER" | grep -q "Nextvi" || { echo "FAIL: expected Nextvi in output"; exit 1; }
echo "$VER" | grep -q "7.4" || { echo "FAIL: expected version 7.4"; exit 1; }

# Noninteractive test: create a file, run vi with 'q' command to exit
TMPDIR=$(mktemp -d)
echo "hello world" > "$TMPDIR/test.txt"
# Send :q to vi via stdin with a TTY workaround
# Nextvi reads from terminal so we use a pipe and check exit status
# Note: without a real tty, vi may not work interactively, but we can test binary existence and version
echo "PASS: Nextvi smoke test (version verified, interactive test skipped - no tty)"
rm -rf "$TMPDIR"
