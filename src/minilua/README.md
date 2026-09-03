# MiniLua

This is Lua contained in a single header to be bundled in C/C++ applications with ease.
[Lua](https://www.lua.org/) is a powerful, efficient, lightweight, embeddable scripting language.

## Example Usage

```c
#define LUA_IMPL
#include "minilua.h"

int main() {
  lua_State *L = luaL_newstate();
  if(L == NULL)
    return -1;
  luaL_openlibs(L);
  luaL_loadstring(L, "print 'hello world'");
  lua_call(L, 0, 0);
  lua_close(L);
  return 0;
}
```

## Usage

Copy `minilua.h` into your C or C++ project, include it anywhere you want to use Lua API.
Then do the following in *one* C file to implement Lua:
```c
#define LUA_IMPL
#include "minilua.h"
```

By default it detects the system platform to use, however you can explicitly define one.

Note that almost no modification was made in the Lua implementation code,
thus there are some C variable names that may collide with your code,
therefore it is best to declare the Lua implementation in dedicated C file.

Optionally provide the following defines:
  - `LUA_MAKE_LUA`     - implement the Lua command line REPL

## Documentation

For documentation on how to use Lua read its [official manual](https://www.lua.org/manual/).

## Updates

- **05-Jan-2026**: Updated to Lua 5.5.0.
- **25-Jul-2024**: Updated to Lua 5.4.7.
- **13-Nov-2023**: Updated to Lua 5.4.6.
- **28-Jan-2022**: Updated to Lua 5.4.4.
- **31-Mar-2021**: Updated to Lua 5.4.3.
- **03-Dec-2020**: Updated to Lua 5.4.2.
- **27-Nov-2020**: Library created, using Lua 5.4.2-rc1.

## Notes

This library tries to keep up with latest official Lua release.
The header is generated using the bash script `gen.sh` all modifications done is there.

## License

Same license as Lua, the MIT license, see LICENSE.txt for information.
