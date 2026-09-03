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

-- [tag:luax-version]

local version = "10.10.0"
local year = 2026
local url = "codeberg.org/cdsoft/luax"
local author = "Christophe Delord"

return setmetatable({
    version = version,
    copyright = ("Copyright (C) 2021-%d %s, %s"):format(year, url, author),
    url = url,
    author = author,
}, {
    __tostring = function() return "LuaX "..version end,
})
