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

local has_sys, sys = pcall(require, "_sys")

if not has_sys then

    sys = {
        libc = "lua",
    }

    local targets = require "luax-targets"

    local kernel, machine

    if package.config:sub(1, 1) == "/" then
        -- Search for a Linux-like target
        kernel, machine = io.popen("uname -s -m", "r") : read "a" : match "(%S+)%s+(%S+)"
    else
        -- Search for a Windows target
        kernel, machine = os.getenv "OS", os.getenv "PROCESSOR_ARCHITECTURE"
    end

    local target
    for i = 1, #targets do
        if targets[i].kernel==kernel and targets[i].machine==machine then
            target = targets[i]
            break
        end
    end

    if not target then
        io.stderr:write("ERROR: Unknown architecture\n",
            "Please report the bug with this information:\n",
            "    config  = "..package.config:lines():head().."\n",
            "    kernel  = "..tostring(kernel).."\n",
            "    machine = "..tostring(machine).."\n",
            ">> https://codeberg.org/cdsoft/luax/issues <<\n"
        )
        os.exit(1)
    end

    sys.name = target.name
    sys.os = target.os
    sys.arch = target.arch
    sys.exe = target.exe
    sys.so = target.so

end

return sys
