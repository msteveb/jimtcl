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
## Shell
@@@]]

--[[@@@
```lua
local sh = require "sh"
```
@@@]]
local sh = {}

local F = require "F"
local sys = require "sys"

local __WINDOWS__ = sys.os == "windows"

--[[@@@
```lua
sh.run(...)
```
> Runs the command `...` with `os.execute`.
@@@]]

function sh.run(...)
    local cmd = F.flatten{...}:unwords()
    return os.execute(cmd)
end

--[[@@@
```lua
sh.read(...)
```
> Runs the command `...` with `io.popen`.
> When `sh.read` succeeds, it returns the content of stdout.
> Otherwise it returns the error identified by `io.popen`.
@@@]]

function sh.read(...)
    local cmd = F.flatten{...}:unwords()
    local p, popen_err = io.popen(cmd, "r")
    if not p then return p, popen_err end
    local out = p:read("a")
    local ok, exit, ret = p:close()
    if ok then
        return out
    else
        return ok, exit, ret
    end
end

--[[@@@
```lua
sh.write(...)(data)
```
> Runs the command `...` with `io.popen` and feeds `stdin` with `data`.
> `sh.write` returns the same values returned by `os.execute`.
@@@]]

function sh.write(...)
    local cmd = F.flatten{...}:unwords()
    return function(data)
        if type(data) ~= "string" then
            return nil, "bad argument #1 to 'write' (string expected, got "..type(data)..")"
        end
        local p, popen_err = io.popen(cmd, "w")
        if not p then return p, popen_err end
        p:write(data)
        return p:close()
    end
end

--[[@@@
```lua
sh.pipe(...)(data)
```
> Runs the command `...` with `io.popen` and feeds `stdin` with `data`.
> When `sh.pipe` succeeds, it returns the content of stdout.
> Otherwise it returns the error identified by `io.popen`.
@@@]]

function sh.pipe(...)
    local cmd = F.flatten{...}
    local cat = __WINDOWS__ and "type" or "cat"
    return function(data)
        local fs = require "fs"
        if type(data) ~= "string" then
            return nil, "bad argument #1 to 'write' (string expected, got "..type(data)..")"
        end
        return fs.with_tmpfile(function(tmp)
            fs.write_bin(tmp, data)
            return sh.read(cat, tmp, " | ", cmd)
        end)
    end
end

--[[@@@
``` lua
sh(...)
```
> `sh` can be called as a function. `sh(...)` is a shortcut to `sh.read(...)`.
@@@]]
setmetatable(sh, {
    __call = function(_, ...) return sh.read(...) end,
})

return sh
