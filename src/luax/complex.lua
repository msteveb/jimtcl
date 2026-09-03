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
local has_complex, complex = pcall(require, "_complex")

if not has_complex then

    -- see https://github.com/krakow10/Complex-Number-Library/blob/master/Lua/Complex.lua

    local mathx = require "mathx"

    local e <const> = math.exp(1)
    local pi <const> = math.pi
    local abs = math.abs
    local exp = math.exp
    local log = math.log
    local cos = math.cos
    local sin = math.sin
    local cosh = mathx.cosh
    local sinh = mathx.sinh
    local atan2 = math.atan

    local mt = {__index={}}

    ---@diagnostic disable:unused-vararg
    local function ni(f) return function(...) error(f.." not implemented") end end

    local forget <const> = 1e-14

    local function new(x, y)
        if forget then
            if x and abs(x) <= forget then x = 0 end
            if y and abs(y) <= forget then y = 0 end
        end
        return setmetatable({x=x or 0, y=y or 0}, mt)
    end

    local i = new(0, 1)

    local function _z(z)
        if type(z) == "table" and getmetatable(z) == mt then return z end
        return new(tonumber(z), 0)
    end

    function mt.__index.real(z) return z.x end

    function mt.__index.imag(z) return z.y end

    local function rect(r, phi)
        return new(r*cos(phi), r*sin(phi))
    end

    local function arg(z)
        return atan2(z.y, z.x)
    end

    local function ln(z)
        return new(log(z.x^2+z.y^2)/2, atan2(z.y, z.x))
    end

    function mt.__index.conj(z)
        return new(z.x, -z.y)
    end

    function mt.__add(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        return new(z1.x+z2.x, z1.y+z2.y)
    end

    function mt.__sub(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        return new(z1.x-z2.x, z1.y-z2.y)
    end

    function mt.__mul(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        return new(z1.x*z2.x-z1.y*z2.y, z1.x*z2.y+z2.x*z1.y)
    end

    function mt.__div(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        local d = z2.x^2 + z2.y^2
        return new((z1.x*z2.x+z1.y*z2.y)/d, (z2.x*z1.y-z1.x*z2.y)/d)
    end

    function mt.__pow(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        local z1sq = z1.x^2 + z1.y^2
        if z1sq == 0 then
            if z2.x == 0 and z2.y == 0 then return 1 end
            return 0
        end
        local phi = arg(z1)
        return rect(z1sq^(z2.x/2)*exp(-z2.y*phi), z2.y*log(z1sq)/2+z2.x*phi)
    end

    function mt.__unm(z)
        return new(-z.x, -z.y)
    end

    function mt.__eq(z1, z2)
        z1 = _z(z1)
        z2 = _z(z2)
        return z1.x == z2.x and z1.y == z2.y
    end

    function mt.__tostring(z)
        if z.y == 0 then return tostring(z.x) end
        if z.x == 0 then
            if z.y == 1 then return "i" end
            if z.y == -1 then return "-i" end
            return z.y.."i"
        end
        if z.y == 1 then return z.x.."+i" end
        if z.y == -1 then return z.x.."-i" end
        if z.y < 0 then return z.x..z.y.."i" end
        return z.x.."+"..z.y.."i"
    end

    function mt.__index.abs(z)
        return (z.x^2+z.y^2)^0.5
    end

    mt.__index.arg = arg

    function mt.__index.exp(z)
        return e^z
    end

    function mt.__index.sqrt(z)
        return z^0.5
    end

    function mt.__index.sin(z)
        return new(sin(z.x)*cosh(z.y), cos(z.x)*sinh(z.y))
    end

    function mt.__index.cos(z)
        return new(cos(z.x)*cosh(z.y), -sin(z.x)*sinh(z.y))
    end

    function mt.__index.tan(z)
        z = 2*z
        local div = cos(z.x) + cosh(z.y)
        return new(sin(z.x)/div, sinh(z.y)/div)
    end

    function mt.__index.sinh(z)
        return new(cos(z.y)*sinh(z.x), sin(z.y)*cosh(z.x))
    end

    function mt.__index.cosh(z)
        return new(cos(z.y)*cosh(z.x), sin(z.y)*sinh(z.x))
    end

    function mt.__index.tanh(z)
        z = 2*z
        local div = cos(z.y) + cosh(z.x)
        return new(sinh(z.x)/div, sin(z.y)/div)
    end

    function mt.__index.asin(z)
        return -i*ln(i*z+(1-z^2)^0.5)
    end

    function mt.__index.acos(z)
        return pi/2 + i*ln(i*z+(1-z^2)^0.5)
    end

    function mt.__index.atan(z)
        local z3, z4 = new(1-z.y, z.x), new(1+z.x^2-z.y^2, 2*z.x*z.y)
        return new(arg(z3/z4^0.5), -log(z3:abs()/z4:abs()^0.5))
    end

    function mt.__index.asinh(z)
        return ln(z+(1+z^2)^0.5)
    end

    function mt.__index.acosh(z)
        return 2*ln((z-1)^0.5+(z+1)^0.5)-log(2)
    end

    function mt.__index.atanh(z)
        return (ln(1+z)-ln(1-z))/2
    end

    mt.__index.log = ln

    mt.__index.proj = ni "proj"

    complex = {
        new = new,
        I = i,
        real = function(z) return _z(z):real() end,
        imag = function(z) return _z(z):imag() end,
        abs = function(z) return _z(z):abs() end,
        arg = function(z) return _z(z):arg() end,
        exp = function(z) return _z(z):exp() end,
        sqrt = function(z) return _z(z):sqrt() end,
        sin = function(z) return _z(z):sin() end,
        cos = function(z) return _z(z):cos() end,
        tan = function(z) return _z(z):tan() end,
        sinh = function(z) return _z(z):sinh() end,
        cosh = function(z) return _z(z):cosh() end,
        tanh = function(z) return _z(z):tanh() end,
        asin = function(z) return _z(z):asin() end,
        acos = function(z) return _z(z):acos() end,
        atan = function(z) return _z(z):atan() end,
        asinh = function(z) return _z(z):asinh() end,
        acosh = function(z) return _z(z):acosh() end,
        atanh = function(z) return _z(z):atanh() end,
        pow = function(z, z2) return _z(z) ^ z2 end,
        log = function(z) return _z(z):log() end,
        proj = function(z) return _z(z):proj() end,
        conj = function(z) return _z(z):conj() end,
        tostring = function(z) return _z(z):tostring() end,
    }

end

return complex
