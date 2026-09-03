#!/bin/sh
# test-luax.sh: Smoke test for LuaX pure-Lua modules
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
LUA="$ROOT/bin/linux-x64/lua"
LUAX_DIR="$ROOT/src/luax"

[ -x "$LUA" ] || { echo "SKIP: Lua not built"; exit 0; }
[ -d "$LUAX_DIR" ] || { echo "SKIP: LuaX not vendored"; exit 0; }

# Add luax to package path
LUAPATH="$LUAX_DIR/?.lua"

# Test strict module
RES="$("$LUA" -e "package.path='$LUAPATH;'..package.path; require('strict'); print('strict ok')")"
echo "$RES" | grep -q "strict ok" || { echo "FAIL: strict module"; exit 1; }

# Test serpent (Lua serializer)
RES="$("$LUA" -e "
package.path='$LUAPATH;'..package.path
local s = require('serpent')
local t = {1, 2, 3}
print(s.line(t))
")"
echo "serpent output: $RES"
echo "$RES" | grep -q "1" || { echo "FAIL: serpent module"; exit 1; }

# Test json module
RES="$("$LUA" -e "
package.path='$LUAPATH;'..package.path
local json = require('json')
local t = json.decode('{\"x\":42}')
print(t.x)
")"
[ "$RES" = "42" ] || { echo "FAIL: json module expected 42, got $RES"; exit 1; }

# Test F (functional) module
RES="$("$LUA" -e "
package.path='$LUAPATH;'..package.path
local F = require('F')
print(type(F))
")"
echo "F module type: $RES"
[ "$RES" = "table" ] || { echo "FAIL: F module expected table, got $RES"; exit 1; }

echo "PASS: LuaX pure-Lua modules smoke test"
