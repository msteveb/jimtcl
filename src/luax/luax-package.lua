--[[
This file is part of luax.

luax is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

luax is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with luax.  If not, see <https://www.gnu.org/licenses/>.

For further information about luax you can visit
https://codeberg.org/cdsoft/luax
--]]

--@LOAD=_

local F = require "F"

local package  = require "package"

-- inspired by https://stackoverflow.com/questions/60283272/how-to-get-the-exact-path-to-the-script-that-was-loaded-in-lua

-- This module wraps package searchers in a function that tracks package paths.
-- The paths are stored in package.modpath, which can be used to generate dependency files
-- for [ypp](https://codeberg.org/cdsoft/luax) or [panda](https://codeberg.org/cdsoft/panda).

--[=[-----------------------------------------------------------------------@@@
# package

The standard Lua package `package` is added some information about packages loaded by LuaX.
@@@]=]

--[[@@@
```lua
package.modpath      -- { module_name = module_path }
```
> table containing the names of the loaded packages and their actual paths.
>
> `package.modpath` contains the names of the packages loaded by `require`, `dofile`, `loadfile`, `import`
> and `toml.parse`.

```lua
package.track(name, [path])     -- package.modpath[name] = path or name
```
> add `name` to `package.modpath`.
@@@]]

package.modpath = F{}

function package.track(name, path)
    package.modpath[name] = path or name
end

local function wrap_searcher(searcher)
    return function(modname)
        local loader, path = searcher(modname)
        if type(loader) == "function" then
            package.track(modname, path)
        end
        return loader, path
    end
end

for i = 2, #package.searchers do
    package.searchers[i] = wrap_searcher(package.searchers[i])
end

local function wrap(func)
    return function(filename, ...)
        if filename ~= nil then
            package.track(filename)
        end
        return func(filename, ...)
    end
end

dofile = wrap(dofile)
loadfile = wrap(loadfile)

local toml = require "toml"
local _toml_parse = toml.parse

---@diagnostic disable-next-line: duplicate-set-field
toml.parse = function(filename, options)
    if not options or not options.load_from_string then
        package.track(filename)
    end
    return _toml_parse(filename, options)
end

return package
