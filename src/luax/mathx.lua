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

local has_mathx, mathx = pcall(require, "_mathx")
if has_mathx then return mathx end

mathx = {}

local exp = math.exp
local log = math.log
local log2 = function(x) return log(x, 2) end
local abs = math.abs
local max = math.max
local floor = math.floor
local ceil = math.ceil
local modf = math.modf

local pack = string.pack
local unpack = string.unpack

local inf <const> = 1/0

---@diagnostic disable:unused-vararg
local function ni(f) return function(...) error(f.." not implemented") end end

local function sign(x) return x < 0 and -1 or 1 end

mathx.fabs = math.abs
mathx.acos = math.acos
mathx.acosh = function(x) return log(x + (x^2-1)^0.5) end
mathx.asin = math.asin
mathx.asinh = function(x) return log(x + (x^2+1)^0.5) end
mathx.atan = math.atan
mathx.atan2 = math.atan
mathx.atanh = function(x) return 0.5*log((1+x)/(1-x)) end
mathx.cbrt = function(x) return x < 0 and -(-x)^(1/3) or x^(1/3) end
mathx.ceil = math.ceil
mathx.copysign = function(x, y) return abs(x) * sign(y) end
mathx.cos = math.cos
mathx.cosh = function(x) return (exp(x)+exp(-x))/2 end
mathx.deg = math.deg
mathx.erf = ni "erf"
mathx.erfc = ni "erfc"
mathx.exp = math.exp
mathx.exp2 = function(x) return 2^x end
mathx.expm1 = function(x) return exp(x)-1 end
mathx.fdim = function(x, y) return max(x-y, 0) end
mathx.floor = math.floor
mathx.fma = function(x, y, z) return x*y + z end
mathx.fmax = math.max
mathx.fmin = math.min
mathx.fmod = math.fmod
mathx.frexp = function(x)
    if x == 0 then return 0, 0 end
    local ax = abs(x)
    local e = ceil(log2(ax))
    local m = ax / (2^e)
    if m == 1 then m, e = m/2, e+1 end
    return m*sign(x), e
end
mathx.gamma = ni "gamma"
mathx.hypot = function(x, y)
    if x == 0 and y == 0 then return 0.0 end
    local ax, ay = abs(x), abs(y)
    if ax > ay then return ax * (1+(y/x)^2)^0.5 end
    return ay * (1+(x/y)^2)^0.5
end
mathx.isfinite = function(x) return abs(x) < inf end
mathx.isinf = function(x) return abs(x) == inf end
mathx.isnan = function(x) return x ~= x end
mathx.isnormal = ni "isnormal"
mathx.ldexp = function(x, e) return x*2^e end
mathx.lgamma = ni "lgamma"
mathx.log = math.log
mathx.log10 = function(x) return log(x, 10) end
mathx.log1p = function(x) return log(1+x) end
mathx.log2 = function(x) return log(x, 2) end
mathx.logb = ni "logb"
mathx.modf = math.modf
mathx.nearbyint = function(x)
    local m = modf(x)
    if m%2 == 0 then
        return x < 0 and floor(x+0.5) or ceil(x-0.5)
    else
        return x >= 0 and floor(x+0.5) or ceil(x-0.5)
    end
end
mathx.nextafter = function(x, y)
    if x == y then return x end
    if x == 0 then
        if y > 0 then return 0x0.0000000000001p-1022 end
        if y < 0 then return -0x0.0000000000001p-1022 end
    end
    local i = unpack("i8", pack("d", x))
    i = i + (  y > x and x < 0 and -1
            or y < x and x < 0 and 1
            or y > x and x > 0 and 1
            or y < x and x > 0 and -1
            )
    return unpack("d", pack("i8", i))
end
mathx.pow = function(x, y) return x^y end
mathx.rad = math.rad
mathx.round = function(x) return x >= 0 and floor(x+0.5) or ceil(x-0.5) end
mathx.scalbn = ni "scalbn"
mathx.sin = math.sin
mathx.sinh = function(x) return (exp(x)-exp(-x))/2 end
mathx.sqrt = math.sqrt
mathx.tan = math.tan
mathx.tanh = function(x) return (exp(x)-exp(-x))/(exp(x)+exp(-x)) end
mathx.trunc = function(x) return x >= 0 and floor(x) or ceil(x) end

mathx.inf = inf
mathx.nan = math.abs(0/0)
mathx.pi = math.pi

return mathx
