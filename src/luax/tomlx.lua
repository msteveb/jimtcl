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
# tomlx

`tomlx` is a layer on top of `toml` ([tinytoml](https://github.com/FourierTransformer/tinytoml)).

It uses Lua as a macro language to transform values.
Macros are string values starting with `=`.
The expression following `=` is a Lua expression which value replaces the macro in the table.

The evaluation environment contains two specific symbols:

- `__up`: environment one level above the current level
- `__root`: root level of the environment levels

```lua
local tomlx = require "tomlx"
```
@@@]]

local tomlx = {}

local F = require "F"
local fs = require "fs"
local toml = require "toml"

local function pattern(options)
    return options and options.pattern or "^=%s*(.-)%s*$"
end

local function chain(env1, env2)
    return setmetatable({}, {
        __index = function(_, k)
            local v = env2[k]
            if v ~= nil then return v end
            return env1[k]
        end
    })
end

local function chain_and_uplink(env1, env2)
    local env = chain(env1, env2)
    env.__up = env1
    return env
end

--[[@@@
The default environment contains the global variables (`_G`)
and some LuaX modules (`crypt`, `F`, `fs`, `sh`).
@@@]]

local default_env = chain({
    crypt = require "crypt",
    F = require "F",
    fs = require "fs",
    sh = require "sh",
}, _G)

local function root_env(t, options)
    local root = chain(default_env, {__root=t})
    local env = options and options.env
    if env then return chain(root, env) end
    return root
end

local function join(path, k)
    if type(k) == "number" then return path.."["..k.."]" end
    if path then return path.."."..k end
    return k
end

local function process(t, env, pat, path)
    local t2 = {}
    env = chain_and_uplink(env, t)
    for k, v in pairs(t) do
        if type(v) == "table" then
            local path2 = join(path, k)
            rawset(t2, k, process(v, env, pat, path2))
        elseif type(v) == "string" then
            local expr = v:match(pat)
            if expr then
                local path2 = join(path, k)
                rawset(t2, k, assert(load("return "..expr, "@"..path2..": "..expr, "t", env))())
            else
                rawset(t2, k, v)
            end
        else
            rawset(t2, k, v)
        end
    end
    return t2
end

local function input_options(options, load_from_string)
    return F.patch(options or {}, {load_from_string=load_from_string})
end

--[[@@@
```lua
tomlx.read(filename, [options])
```
> calls `toml.parse` to parse a TOML file.
> Options are optional
> and described in the [tinytoml documentation](https://github.com/FourierTransformer/tinytoml?tab=readme-ov-file#parsing-toml).
> tomlx adds the env option (`options.env`) to define the initial evaluation environment.
> The table returned by `tinytoml` is then processed to evaluate `tomlx` macros.
@@@]]
function tomlx.read(filename, options)
    local t = toml.parse(filename, input_options(options, false))
    return process(t, root_env(t, options), pattern(options))
end

--[[@@@
```lua
tomlx.decode(s, [options])
```
> calls `toml.parse` to parse a TOML string.
> Options are optional
> and described in the [tinytoml documentation](https://github.com/FourierTransformer/tinytoml?tab=readme-ov-file#parsing-toml).
> tomlx adds the env option (`options.env`) to define the initial evaluation environment.
> The table returned by `tinytoml` is then processed to evaluate `tomlx` macros.
@@@]]
function tomlx.decode(s, options)
    local t = toml.parse(s, input_options(options, true))
    return process(t, root_env(t, options), pattern(options))
end

--[[@@@
```lua
tomlx.encode(s, [options])
```
> calls `toml.encode` to encode a Lua table into a TOML string.
> Options are optional
> and described in the [tinytoml documentation](https://github.com/FourierTransformer/tinytoml?tab=readme-ov-file#encoding-toml).
@@@]]
function tomlx.encode(t, options)
    return toml.encode(t, options)
end

--[[@@@
```lua
tomlx.write(filename, t, [options])
```
> calls `toml.encode` to encode a Lua table into a TOML string
> and save it the file `filename`.
> Options are optional
> and described in the [tinytoml documentation](https://github.com/FourierTransformer/tinytoml?tab=readme-ov-file#encoding-toml).
@@@]]
function tomlx.write(filename, t, options)
    fs.write(filename, toml.encode(t, options))
end

--[[@@@
```lua
tomlx.validate(schema, filename, [options])
```
> returns `true` if `filename` is validated by `schema`. Otherwise it returns `false`
> and a list of failures.
>
> The `schema` file is a TOML file used to validate the TOML file `filename`.
> Both files are read with `tomlx.read` and the corresponding tables are validated with `F.validate`.
>
> Options (the `option` table) contains options for `tomlx.read` and `F.validate`.
@@@]]
function tomlx.validate(schema, filename, options)
    return F.validate(tomlx.read(schema, options), tomlx.read(filename, options), options)
end

return tomlx
