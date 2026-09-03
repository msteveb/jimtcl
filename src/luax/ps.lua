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

local has_ps, ps = pcall(require, "_ps")

if not has_ps then

    ps = {}

    local pack, unpack = table.pack, table.unpack
    local clock = os.clock

    function ps.sleep(n)
        io.popen("sleep "..tostring(n)):close()
    end

    ps.time = os.time

    ps.clock = clock

    function ps.profile(func, ...)
        local t0 = clock()
        local results = pack(pcall(func, ...))
        local t1 = clock()
        if results[1] then
            return t1 - t0, unpack(results, 2, results.n)
        else
            return results[1], results[2]
        end
    end

end

return ps
