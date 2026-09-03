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

-- This module adds some functions to the debug package.

--[=[-----------------------------------------------------------------------@@@
# debug

The standard Lua package `debug` is added some functions to help debugging.
@@@]=]

--[[@@@
```lua
debug.locals(level)
```
> table containing the local variables at a given level `level`.
  The default level is the caller level (1).
  If `level` is a function, `locals` returns the names of the function parameters.
@@@]]

function debug.locals(level)
    local vars = F{}
    if type(level) == "function" then
        local i = 1
        while true do
            local name = debug.getlocal(level, i)
            if name==nil then break end
            if not name:match "^%(" then
                vars[#vars+1] = name
            end
            i = i+1
        end
    else
        level = (level or 1) + 1
        local i = 1
        while true do
            local name, val = debug.getlocal(level, i)
            if name==nil then break end
            if not name:match "^%(" then
                vars[name] = val
            end
            i = i+1
        end
    end
    return vars
end

return debug
