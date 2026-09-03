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

--@LIB

--[[------------------------------------------------------------------------@@@
# import: import Lua scripts into tables

```lua
local import = require "import"
```

The import module can be used to manage simple configuration files,
configuration parameters being global variables defined in the configuration file.

```lua
local conf = import("myconf.lua", [env])
```
> Evaluates `"myconf.lua"` in a new table and returns this table.
> All files are tracked in `package.modpath`.
>
> The execution environment inherits from `env` (or `_ENV` if `env` is not defined).
@@@]]

return function(fname, env)
    local mod = setmetatable({}, {__index = env or _ENV})
    assert(loadfile(fname, "t", mod))()
    return mod
end
