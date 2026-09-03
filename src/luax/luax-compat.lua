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

-- Lua 5.4 / 5.5 compatibility

local function lua54()

    local mathx = require "mathx"

    ---------------------------------------------------------------------------
    -- missing math functions
    ---------------------------------------------------------------------------

    math.frexp = mathx.frexp
    math.ldexp = mathx.ldexp

    ---------------------------------------------------------------------------
    -- missing table functions
    ---------------------------------------------------------------------------

    ---@diagnostic disable-next-line: duplicate-set-field
    table.create = function(nseq, nrec) return {} end ---@diagnostic disable-line: unused-local

end

local function lua55()
end

if _VERSION == "Lua 5.4" then return lua54() end
if _VERSION == "Lua 5.5" then return lua55() end

io.stderr:write(_VERSION, ": unsupported Lua version\n")
