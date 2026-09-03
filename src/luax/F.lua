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

-- Load F.lua to add new methods to strings
--@LOAD=_

--[[------------------------------------------------------------------------@@@
# Functional programming utilities

```lua
local F = require "F"
```

`F` provides some useful functions inspired by functional programming languages,
especially by these Haskell modules:

- [`Data.List`](https://hackage.haskell.org/package/base-4.17.0.0/docs/Data-List.html)
- [`Data.Map`](https://hackage.haskell.org/package/containers-0.6.6/docs/Data-Map.html)
- [`Data.String`](https://hackage.haskell.org/package/base-4.17.0.0/docs/Data-String.html)
- [`Prelude`](https://hackage.haskell.org/package/base-4.17.0.0/docs/Prelude.html)

@@@]]

--[[@@@
This module provides functions for Lua tables that can represent both arrays (i.e. integral indices)
and tables (i.e. any indice types).
The `F` constructor adds methods to tables which may interfere with table fields that could have the same names.
In this case, F also defines the `__call` metamethod that takes a method name
and returns a function that actually calls the requested method.
E.g.:

```lua
t = F{foo = 12, mapk = 42} -- note that mapk is also a method of F tables

t:mapk(func)    -- fails because mapk is a field of t
t"mapk"(func)   -- works and is equivalent to F.mapk(func, t)
```

@@@]]

require "luax-compat"

local getmetatable, setmetatable = getmetatable, setmetatable
local ipairs, pairs, next = ipairs, pairs, next
local load = load
local pcall = pcall
local rawequal, rawset = rawequal, rawset
local select = select
local tostring = tostring
local type = type

local t_create = table.create
local t_concat = table.concat
local t_insert, t_remove, t_move = table.insert, table.remove, table.move
local t_sort = table.sort
local t_pack, t_unpack = table.pack, table.unpack

local function t_create_safe(nseq)
    if nseq <= 0 then return {} end
    return t_create(nseq)
end

local s_byte, s_char = string.byte, string.char
local s_find, s_match, s_gmatch, s_gsub = string.find, string.match, string.gmatch, string.gsub
local s_format = string.format
local s_lower, s_upper = string.lower, string.upper
local s_rep = string.rep
local s_sub = string.sub

local s_bytes, s_chars
local s_cap
local s_take, s_drop
local s_head, s_tail
local s_init, s_last
local s_split

local abs = math.abs
local ceil = math.ceil
local fmod = math.fmod
local math_type = math.type
local max = math.max

local mathx = require "mathx"
local copysign = mathx.copysign
local isnormal = mathx.isnormal

local F = {}

local mt = {
    __name = "F-table",
    __index = {},
}

function mt.__call(self, name)
    local method = mt.__index[name]
    return function(...) return method(self, ...) end
end

local F_clone
local F_concat, F_merge
local F_pairs
local F_keys
local F_head, F_tail
local F_init, F_last
local F_take, F_drop
local F_take_while, F_drop_while, F_drop_while_end
local Nil
local F_comp, F_op_eq, F_op_lt
local F_quot_rem, F_div_mod
local F_is_prefix_of, F_is_suffix_of, F_is_infix_of
local F_const
local F_length
local F_null
local F_map, F_from_set
local F_foreach
local F_filterk
local F_flatten
local F_zip
local F_minimum
local F_permutations

--[[------------------------------------------------------------------------@@@
## Standard types, and related functions
@@@]]

local type_rank = {
    ["nil"]         = 0,
    ["number"]      = 1,
    ["string"]      = 2,
    ["boolean"]     = 3,
    ["table"]       = 4,
    ["function"]    = 5,
    ["thread"]      = 6,
    ["userdata"]    = 7,
}

local function universal_eq(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return false end
    if ta == "nil" then return true end
    if ta == "table" then
        local ks = F_keys(F_merge{a, b})
        for i = 1, #ks do
            local k = ks[i]
            if not universal_eq(a[k], b[k]) then return false end
        end
        return true
    end
    return a == b
end

local function universal_ne(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return true end
    if ta == "nil" then return false end
    if ta == "table" then
        local ks = F_keys(F_merge{a, b})
        for i = 1, #ks do
            local k = ks[i]
            if universal_ne(a[k], b[k]) then return true end
        end
        return false
    end
    return a ~= b
end

local function universal_comp(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return F_comp(type_rank[ta], type_rank[tb]) end
    if ta == "nil" then return 0 end
    if ta == "number" or ta == "string" or ta == "boolean" then return F_comp(a, b) end
    if ta == "table" then
        local ks = F_keys(F_merge{a, b})
        for i = 1, #ks do
            local k = ks[i]
            local ak = a[k]
            local bk = b[k]
            local order = universal_comp(ak, bk)
            if order ~= 0 then return order end
        end
        return 0
    end
    return F_comp(tostring(a), tostring(b))
end

local function universal_lt(a, b)
    return universal_comp(a, b) < 0
end

local function universal_le(a, b)
    return universal_comp(a, b) <= 0
end

local function universal_gt(a, b)
    return universal_comp(a, b) > 0
end

local function universal_ge(a, b)
    return universal_comp(a, b) >= 0
end

local function key_eq(a, b)
    return a == b
end

local function key_ne(a, b)
    return a ~= b
end

local function key_lt(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return type_rank[ta] < type_rank[tb] end
    return a < b
end

local function key_le(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return type_rank[ta] <= type_rank[tb] end
    return a <= b
end

local function key_gt(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return type_rank[ta] > type_rank[tb] end
    return a > b
end

local function key_ge(a, b)
    local ta, tb = type(a), type(b)
    if ta ~= tb then return type_rank[ta] >= type_rank[tb] end
    return a >= b
end

--[[------------------------------------------------------------------------@@@
### Operators
@@@]]

F.op = {}

--[[@@@
```lua
F.op.land(a, b)             -- a and b
F.op.lor(a, b)              -- a or b
F.op.lxor(a, b)             -- (not a and b) or (not b and a)
F.op.lnot(a)                -- not a
```
> Logical operators
@@@]]

F.op.land = function(a, b) return a and b end
F.op.lor = function(a, b) return a or b end
F.op.lxor = function(a, b) return (not a and b) or (not b and a) end
F.op.lnot = function(a) return not a end

--[[@@@
```lua
F.op.band(a, b)             -- a & b
F.op.bor(a, b)              -- a | b
F.op.bxor(a, b)             -- a ~ b
F.op.bnot(a)                -- ~a
F.op.shl(a, b)              -- a << b
F.op.shr(a, b)              -- a >> b
```
> Bitwise operators
@@@]]

F.op.band = function(a, b) return a & b end
F.op.bor = function(a, b) return a | b end
F.op.bxor = function(a, b) return a ~ b end
F.op.bnot = function(a) return ~a end
F.op.shl = function(a, b) return a << b end
F.op.shr = function(a, b) return a >> b end

--[[@@@
```lua
F.op.eq(a, b)               -- a == b
F.op.ne(a, b)               -- a ~= b
F.op.lt(a, b)               -- a < b
F.op.le(a, b)               -- a <= b
F.op.gt(a, b)               -- a > b
F.op.ge(a, b)               -- a >= b
```
> Comparison operators
@@@]]

F.op.eq = function(a, b) return a == b end
F.op.ne = function(a, b) return a ~= b end
F.op.lt = function(a, b) return a < b end
F.op.le = function(a, b) return a <= b end
F.op.gt = function(a, b) return a > b end
F.op.ge = function(a, b) return a >= b end

F_op_eq = F.op.eq
F_op_lt = F.op.lt

--[[@@@
```lua
F.op.ueq(a, b)              -- a == b  (†)
F.op.une(a, b)              -- a ~= b  (†)
F.op.ult(a, b)              -- a < b   (†)
F.op.ule(a, b)              -- a <= b  (†)
F.op.ugt(a, b)              -- a > b   (†)
F.op.uge(a, b)              -- a >= b  (†)
```
> Universal comparison operators
  ((†) recursive comparisons on elements of possibly different Lua types)
@@@]]

F.op.ueq = universal_eq
F.op.une = universal_ne
F.op.ult = universal_lt
F.op.ule = universal_le
F.op.ugt = universal_gt
F.op.uge = universal_ge

--[[@@@
```lua
F.op.keq(a, b)              -- a == b  (†)
F.op.kne(a, b)              -- a ~= b  (†)
F.op.klt(a, b)              -- a < b   (†)
F.op.kle(a, b)              -- a <= b  (†)
F.op.kgt(a, b)              -- a > b   (†)
F.op.kge(a, b)              -- a >= b  (†)
```
> Universal comparison operators
  ((†) non recursive comparisons on elements of possibly different Lua types).
  The `kxx` functions are faster but less generic than `uxx`.
  They are more suitable for sorting keys (e.g. F.keys).
@@@]]

F.op.keq = key_eq
F.op.kne = key_ne
F.op.klt = key_lt
F.op.kle = key_le
F.op.kgt = key_gt
F.op.kge = key_ge

--[[@@@
```lua
F.op.add(a, b)              -- a + b
F.op.sub(a, b)              -- a - b
F.op.mul(a, b)              -- a * b
F.op.div(a, b)              -- a / b
F.op.idiv(a, b)             -- a // b
F.op.mod(a, b)              -- a % b
F.op.neg(a)                 -- -a
F.op.pow(a, b)              -- a ^ b
```
> Arithmetic operators
@@@]]

F.op.add = function(a, b) return a + b end
F.op.sub = function(a, b) return a - b end
F.op.mul = function(a, b) return a * b end
F.op.div = function(a, b) return a / b end
F.op.idiv = function(a, b) return a // b end
F.op.mod = function(a, b) return a % b end
F.op.neg = function(a) return -a end
F.op.pow = function(a, b) return a ^ b end

--[[@@@
```lua
F.op.concat(a, b)           -- a .. b
F.op.len(a)                 -- #a
```
> String/list operators
@@@]]

F.op.concat = function(a, b) return a..b end
F.op.len = function(a) return #a end

--[[------------------------------------------------------------------------@@@
### Basic data types
@@@]]

--[[@@@
```lua
F.maybe(b, f, a)
```
> Returns f(a) if f(a) is not nil, otherwise b
@@@]]
function F.maybe(b, f, a)
    local v = f(a)
    if v == nil then return b end
    return v
end

--[[@@@
```lua
F.default(def, x)
```
> Returns x if x is not nil, otherwise def
@@@]]
function F.default(def, x)
    if x == nil then return def end
    return x
end

--[[@@@
```lua
F.case(x) {
    t1 = v1,
    ...
    tn = vn,
    [F.Nil] = default_value,
}
```
> returns `vi` such that `ti == x` or `default_value` if no `ti` is equal to `x`.
@@@]]

function F.case(val)
    return function(cases)
        local x = cases[val]
        if x ~= nil then return x end
        return cases[Nil]
    end
end

--[[------------------------------------------------------------------------@@@
#### Tuples
@@@]]

--[[@@@
```lua
F.fst(xs)
xs:fst()
```
> Extract the first component of a list.
@@@]]

function F.fst(xs) return xs[1] end
mt.__index.fst = F.fst

--[[@@@
```lua
F.snd(xs)
xs:snd()
```
> Extract the second component of a list.
@@@]]

function F.snd(xs) return xs[2] end
mt.__index.snd = F.snd

--[[@@@
```lua
F.thd(xs)
xs:thd()
```
> Extract the third component of a list.
@@@]]

function F.thd(xs) return xs[3] end
mt.__index.thd = F.thd

--[[@@@
```lua
F.nth(n, xs)
xs:nth(n)
```
> Extract the n-th component of a list.
@@@]]

function F.nth(n, xs) return xs[n] end
function mt.__index:nth(n) return F.nth(n, self) end

--[[------------------------------------------------------------------------@@@
### Basic type classes
@@@]]

--[[@@@
```lua
F.comp(a, b)
```
> Comparison (-1, 0, 1)
@@@]]

F_comp = function(a, b)
    if a < b then return -1 end
    if a > b then return 1 end
    return 0
end
F.comp = F_comp

--[[@@@
```lua
F.ucomp(a, b)
```
> Comparison (-1, 0, 1) (using universal comparison operators)
@@@]]

F.ucomp = universal_comp

--[[@@@
```lua
F.max(a, b)
```
> max(a, b)
@@@]]

function F.max(a, b) if a >= b then return a else return b end end

--[[@@@
```lua
F.min(a, b)
```
> min(a, b)
@@@]]

function F.min(a, b) if a <= b then return a else return b end end

--[[@@@
```lua
F.succ(a)
```
> a + 1
@@@]]

function F.succ(a) return a + 1 end

--[[@@@
```lua
F.pred(a)
```
> a - 1
@@@]]

function F.pred(a) return a - 1 end

--[[------------------------------------------------------------------------@@@
### Numbers
@@@]]
--
--[[------------------------------------------------------------------------@@@
#### Numeric type classes
@@@]]

--[[@@@
```lua
F.negate(a)
```
> -a
@@@]]
function F.negate(a) return -a end

--[[@@@
```lua
F.abs(a)
```
> absolute value of a
@@@]]
F.abs = math.abs

--[[@@@
```lua
F.signum(a)
```
> sign of a (-1, 0 or +1)
@@@]]
function F.signum(a) return F_comp(a, 0) end

--[[@@@
```lua
F.quot(a, b)
```
> integer division truncated toward zero
@@@]]

function F.quot(a, b)
    return (a - fmod(a, b)) // b
end

--[[@@@
```lua
F.rem(a, b)
```
> integer remainder satisfying quot(a, b)*b + rem(a, b) == a, 0 <= rem(a, b) < abs(b)
@@@]]
F.rem = fmod

--[[@@@
```lua
F.quot_rem(a, b)
```
> simultaneous quot and rem
@@@]]

F_quot_rem = function(a, b)
    local r = fmod(a, b)
    local q = (a - r) // b
    return q, r
end
F.quot_rem = F_quot_rem

--[[@@@
```lua
F.div(a, b)
```
> integer division truncated toward negative infinity
@@@]]

function F.div(a, b)
    return a // b
end

--[[@@@
```lua
F.mod(a, b)
```
> integer modulus satisfying div(a, b)*b + mod(a, b) == a, 0 <= mod(a, b) < abs(b)
@@@]]

function F.mod(a, b)
    return a - b*(a//b)
end

--[[@@@
```lua
F.div_mod(a, b)
```
> simultaneous div and mod
@@@]]

F_div_mod = function(a, b)
    local q = a // b
    local r = a - b*q
    return q, r
end
F.div_mod = F_div_mod

--[[@@@
```lua
F.recip(a)
```
> Reciprocal fraction.
@@@]]

function F.recip(a) return 1 / a end

--[[@@@
```lua
F.pi
F.exp(x)
F.log(x), F.log(x, base)
F.log10(x), F.log2(x)
F.sqrt(x)
F.sin(x)
F.cos(x)
F.tan(x)
F.asin(x)
F.acos(x)
F.atan(x)
F.sinh(x)
F.cosh(x)
F.tanh(x)
F.asinh(x)
F.acosh(x)
F.atanh(x)
```
> standard math constants and functions
@@@]]
F.pi = math.pi
F.exp = math.exp
F.log = math.log
F.log10 = mathx.log10
F.log2 = mathx.log2
F.sqrt = math.sqrt
F.sin = math.sin
F.cos = math.cos
F.tan = math.tan
F.asin = math.asin
F.acos = math.acos
F.atan = math.atan
F.sinh = mathx.sinh
F.cosh = mathx.cosh
F.tanh = mathx.tanh
F.asinh = mathx.asinh
F.acosh = mathx.acosh
F.atanh = mathx.atanh

--[[@@@
```lua
F.proper_fraction(x)
```
> returns a pair (n, f) such that x = n+f, and:
>
> - n is an integral number with the same sign as x
> - f is a fraction with the same type and sign as x, and with absolute value less than 1.
@@@]]
F.proper_fraction = math.modf

--[[@@@
```lua
F.truncate(x)
```
> returns the integer nearest x between zero and x and the fractional part of x.
@@@]]
F.truncate = math.modf

--[[@@@
```lua
F.round(x)
```
> returns the nearest integer to x; the even integer if x is equidistant between two integers
@@@]]

F.round = mathx.round

--[[@@@
```lua
F.ceiling(x)
F.ceil(x)
```
> returns the least integer not less than x.
@@@]]
F.ceiling = math.ceil
F.ceil = F.ceiling

--[[@@@
```lua
F.floor(x)
```
> returns the greatest integer not greater than x.
@@@]]
F.floor = math.floor

--[[@@@
```lua
F.is_nan(x)
```
> True if the argument is an IEEE "not-a-number" (NaN) value
@@@]]

F.is_nan = mathx.isnan

--[[@@@
```lua
F.is_infinite(x)
```
> True if the argument is an IEEE infinity or negative infinity
@@@]]

F.is_infinite = mathx.isinf

--[[@@@
```lua
F.is_normalized(x)
```
> True if the argument is represented in normalized format
@@@]]

F.is_normalized = mathx.isnormal

--[[@@@
```lua
F.is_denormalized(x)
```
> True if the argument is too small to be represented in normalized format
@@@]]

function F.is_denormalized(x)
    return not isnormal(x)
end

--[[@@@
```lua
F.is_negative_zero(x)
```
> True if the argument is an IEEE negative zero
@@@]]

function F.is_negative_zero(x)
    return x == 0.0 and copysign(1, x) < 0
end

--[[@@@
```lua
F.atan2(y, x)
```
> computes the angle (from the positive x-axis) of the vector from the origin to the point (x, y).
@@@]]

F.atan2 = mathx.atan

--[[@@@
```lua
F.even(n)
F.odd(n)
```
> parity check
@@@]]

function F.even(n) return n&1 == 0 end

function F.odd(n) return n&1 == 1 end

--[[@@@
```lua
F.gcd(a, b)
F.lcm(a, b)
```
> Greatest Common Divisor and Least Common Multiple of a and b.
@@@]]

do

local function gcd(a, b)
    a, b = abs(a), abs(b)
    while b > 0 do
        a, b = b, a%b
    end
    return a
end

local function lcm(a, b)
    return abs(a // gcd(a, b) * b)
end

F.gcd, F.lcm = gcd, lcm

end

--[[------------------------------------------------------------------------@@@
### Miscellaneous functions
@@@]]

--[[@@@
```lua
F.id(x)
```
> Identity function.
@@@]]

function F.id(...) return ... end

--[[@@@
```lua
F.const(...)
```
> Constant function. const(...)(y) always returns ...
@@@]]

F_const = function(...)
    local val = t_pack(...)
    return function(...) ---@diagnostic disable-line:unused-vararg
        return t_unpack(val)
    end
end
F.const = F_const

--[[@@@
```lua
F.compose(fs)
```
> Function composition. compose{f, g, h}(...) returns f(g(h(...))).
@@@]]

function F.compose(fs)
    local n = #fs
    local function apply(i, ...)
        if i > 0 then return apply(i-1, fs[i](...)) end
        return ...
    end
    return function(...)
        return apply(n, ...)
    end
end

--[[@@@
```lua
F.flip(f)
```
> takes its (first) two arguments in the reverse order of f.
@@@]]

function F.flip(f)
    return function(a, b, ...)
        return f(b, a, ...)
    end
end

--[[@@@
```lua
F.curry(f)
```
> curry(f)(x)(...) calls f(x, ...)
@@@]]

function F.curry(f)
    return function(x)
        return function(...)
            return f(x, ...)
        end
    end
end

--[[@@@
```lua
F.uncurry(f)
```
> uncurry(f)(x, ...) calls f(x)(...)
@@@]]

function F.uncurry(f)
    return function(x, ...)
        return f(x)(...)
    end
end

--[[@@@
```lua
F.partial(f, ...)
```
> F.partial(f, xs)(ys) calls f(xs..ys)
@@@]]

function F.partial(f, ...)
    local n = select("#", ...)
    if n == 0 then return f end
    if n == 1 then local x1                     = ... return function(...) return f(x1, ...) end end
    if n == 2 then local x1, x2                 = ... return function(...) return f(x1, x2, ...) end end
    if n == 3 then local x1, x2, x3             = ... return function(...) return f(x1, x2, x3, ...) end end
    if n == 4 then local x1, x2, x3, x4         = ... return function(...) return f(x1, x2, x3, x4, ...) end end
    if n == 5 then local x1, x2, x3, x4, x5     = ... return function(...) return f(x1, x2, x3, x4, x5, ...) end end
    if n == 6 then local x1, x2, x3, x4, x5, x6 = ... return function(...) return f(x1, x2, x3, x4, x5, x6, ...) end end
    local xs = t_pack(...)
    local xn = xs.n
    return function(...)
        local ys = t_pack(...)
        local yn = ys.n
        t_move(ys, 1, yn, xn+1, xs)
        return f(t_unpack(xs, 1, xn+yn))
    end
end

--[[@@@
```lua
F.call(f, ...)
```
> calls `f(...)`
@@@]]

function F.call(f, ...)
    return f(...)
end

--[[@@@
```lua
F.until_(p, f, x)
```
> yields the result of applying f until p holds.
@@@]]

function F.until_(p, f, x)
    while not p(x) do
        x = f(x)
    end
    return x
end

--[[@@@
```lua
F.error(message, level)
F.error_without_stack_trace(message, level)
```
> stops execution and displays an error message (with out without a stack trace).
@@@]]

do

local function err(msg, level, tb)
    level = (level or 1) + 2
    local info = debug.getinfo(level)
    local file = info.short_src
    local line = info.currentline

    msg = (function()
        if type(msg) == "string" then return msg end
        local msg_mt = getmetatable(msg)
        if msg_mt and msg_mt.__tostring then
            local str = tostring(msg)
            if type(str) == "string" then return str end
        end
        return string.format("(error object is a %s value)", type(msg))
    end)()

    msg = t_concat{arg[0] or "LuaX", ": ", file, ":", line, ": ", msg}
    io.stderr:write(tb and debug.traceback(msg, level) or msg, "\n")
    os.exit(1)
end

function F.error(message, level) err(message, level, true) end

function F.error_without_stack_trace(message, level) err(message, level, false) end

end

--[[@@@
```lua
F.prefix(pre)
```
> returns a function that adds the prefix pre to a string
@@@]]

function F.prefix(pre)
    return function(s) return pre..s end
end

--[[@@@
```lua
F.suffix(suf)
```
> returns a function that adds the suffix suf to a string
@@@]]

function F.suffix(suf)
    return function(s) return s..suf end
end

--[[@@@
```lua
F.memo1(f)
```
> returns a memoized function (one argument)
>
> Note that the memoized function has a `reset` method to forget all the previously computed values.
@@@]]

function F.memo1(f)
    local mem = {}
    return setmetatable({}, {
        __index = {
            reset = function(_) mem = {} end,
        },
        __call = function(_, k)
            local v = mem[k]
            if v then return t_unpack(v) end
            v = t_pack(f(k))
            mem[k] = v
            return t_unpack(v)
        end,
    })
end

--[[@@@
```lua
F.memo(f)
```
> returns a memoized function (any number of arguments)
>
> Note that the memoized function has a `reset` method to forget all the previously computed values.
@@@]]

function F.memo(f)
    local _nil = {}
    local _value = {}
    local mem = {}
    return setmetatable({}, {
        __index = {
            reset = function(_) mem = {} end,
        },
        __call = function(_, ...)
            local cur = mem
            for i = 1, select("#", ...) do
                local k = select(i, ...) or _nil
                cur[k] = cur[k] or {}
                cur = cur[k]
            end
            cur[_value] = cur[_value] or t_pack(f(...))
            return t_unpack(cur[_value])
        end,
    })
end

--[[------------------------------------------------------------------------@@@
## Converting to and from string
@@@]]

--[[------------------------------------------------------------------------@@@
### Converting to string
@@@]]

--[[@@@
```lua
F.show(x, [opt])
```
> Convert x to a string
>
> `opt` is an optional table that customizes the output string:
>
>   - `opt.int`: integer format
>   - `opt.flt`: floating point number format
>   - `opt.indent`: number of spaces use to indent tables (`nil` for a single line output)
@@@]]

do

local default_show_options = {
    int = "%s",
    flt = "%s",
    indent = nil,
    lt = F.op.klt,
}

local keywords = {
    ["and"]=true,       ["break"]=true,     ["do"]=true,        ["else"]=true,      ["elseif"]=true,    ["end"]=true,
    ["false"]=true,     ["for"]=true,       ["function"]=true,  ["global"]=true,    ["goto"]=true,      ["if"]=true,
    ["in"]=true,        ["local"]=true,     ["nil"]=true,       ["not"]=true,       ["or"]=true,        ["repeat"]=true,
    ["return"]=true,    ["then"]=true,      ["true"]=true,      ["until"]=true,     ["while"]=true,
}

function F.show(x, opt)

    opt = F_merge{default_show_options, opt}
    local opt_indent = opt.indent
    local opt_int = opt.int
    local opt_flt = opt.flt
    local opt_lt = opt.lt

    local tokens = {}
    local function emit(token) tokens[#tokens+1] = token end
    local function drop() t_remove(tokens) end

    local stack = {}
    local function push(val) stack[#stack + 1] = val end
    local function pop() t_remove(stack) end
    local function in_stack(val)
        for i = 1, #stack do
            if rawequal(stack[i], val) then return true end
        end
    end

    local tabs = 0

    local function fmt(val)
        if type(val) == "table" then
            if in_stack(val) then
                emit "{...}" -- recursive table
            else
                push(val)
                local need_nl = false
                emit "{"
                if opt_indent then tabs = tabs + opt_indent end
                local n = #val
                for i = 1, n do
                    fmt(val[i])
                    emit ", "
                end
                local first_field = true
                for k, v in F_pairs(val, opt_lt) do
                    if not (type(k) == "number" and math_type(k) == "integer" and 1 <= k and k <= #val) then
                        if first_field and opt_indent and n > 1 then drop() emit "," end
                        first_field = false
                        need_nl = opt_indent ~= nil
                        if opt_indent then emit "\n" emit(s_rep(" ", tabs)) end
                        if type(k) == "string" and s_match(k, "^[%a_][%w_]*$") and not keywords[k] then
                            emit(k)
                        else
                            emit "[" fmt(k) emit "]"
                        end
                        if opt_indent then emit " = " else emit "=" end
                        fmt(v)
                        if opt_indent then emit "," else emit ", " end
                        n = n + 1
                    end
                end
                if n > 0 and not need_nl then drop() end
                if need_nl then emit "\n" end
                if opt_indent then tabs = tabs - opt_indent end
                if opt_indent and need_nl then emit(s_rep(" ", tabs)) end
                emit "}"
                pop()
            end
        elseif type(val) == "number" then
            if math_type(val) == "integer" then
                emit(s_format(opt_int, val))
            elseif math_type(val) == "float" then
                emit(s_gsub(s_format(opt_flt, val), ",", "."))
            else
                emit(s_format("%s", val))
            end
        elseif type(val) == "string" then
            emit(s_format("%q", val))
        else
            emit(s_format("%s", val))
        end
    end

    fmt(x)
    return t_concat(tokens)

end
mt.__index.show = F.show

end

--[[------------------------------------------------------------------------@@@
### Converting from string
@@@]]

--[[@@@
```lua
F.read(s)
```
> Convert s to a Lua value
@@@]]

function F.read(s)
    local chunk, msg = load("return "..s)
    if chunk == nil then return nil, msg end
    local ret = t_pack(pcall(chunk))
    local status, value = ret[1], t_pack(t_unpack(ret, 2, ret.n))
    if not status then return nil, t_unpack(value) end
    return t_unpack(value)
end

--[[------------------------------------------------------------------------@@@
## Table construction
@@@]]

--[[@@@
```lua
F(t)
```
> `F(t)` sets the metatable of `t` and returns `t`.
> Most of the functions of `F` will be methods of `t`.
>
> Note that other `F` functions that return tables actually return `F` tables.
@@@]]

--[[@@@
```lua
F.clone(t)
t:clone()
```
> `F.clone(t)` clones the first level of `t`.
@@@]]

F_clone = function(t)
    local t2 = {}
    for k, v in pairs(t) do t2[k] = v end
    return setmetatable(t2, mt)
end
F.clone = F_clone
mt.__index.clone = F_clone

--[[@@@
```lua
F.deep_clone(t)
t:deep_clone()
```
> `F.deep_clone(t)` recursively clones `t`.
@@@]]

function F.deep_clone(t)
    local function go(t1)
        if type(t1) ~= "table" then return t1 end
        local t2 = {}
        for k, v in pairs(t1) do t2[k] = go(v) end
        return setmetatable(t2, getmetatable(t1))
    end
    return setmetatable(go(t), mt)
end
mt.__index.deep_clone = F.deep_clone

--[[@@@
```lua
F.rep(n, x)
```
> Returns a list of length n with x the value of every element.
@@@]]

function F.rep(n, x)
    local xs = t_create_safe(n)
    for i = 1, n do
        xs[i] = x
    end
    return setmetatable(xs, mt)
end

--[[@@@
```lua
F.range(a)
F.range(a, b)
F.range(a, b, step)
```
> Returns a range [1, a], [a, b] or [a, a+step, ... b]
@@@]]

function F.range(a, b, step)
    step = step or 1
    if step == 0 then return nil, "range step can not be zero" end
    if b == nil then a, b = 1, a end
    local r = t_create_safe(ceil((b-a)/step) + 1)
    for x = a, b, step do r[#r+1] = x end
    return setmetatable(r, mt)
end

--[[@@@
```lua
F.concat{xs1, xs2, ... xsn}
F{xs1, xs2, ... xsn}:concat()
xs1 .. xs2
```
> concatenates lists
@@@]]

F_concat = function(xss)
    local ys = t_create(F_map(F.op.len, xss):sum())
    for i = 1, #xss do
        local xs = xss[i]
        t_move(xs, 1, #xs, #ys+1, ys)
    end
    return setmetatable(ys, mt)
end
F.concat = F_concat
mt.__index.concat = F_concat

function mt.__concat(xs1, xs2)
    return F_concat{xs1, xs2}
end

--[[@@@
```lua
F.flatten(xs, [leaf])
xs:flatten([leaf])
```
> Returns a flat list with all elements recursively taken from xs.
  The optional `leaf` parameter is a predicate that can stop `flatten` on some kind of nodes.
  By default, `flatten` only recurses on tables with no metatable or on `F` tables.
@@@]]

do

local function default_leaf(x)
    -- by default, a table is a leaf if it has a metatable and is not an F' list
    local xmt = getmetatable(x)
    return xmt and xmt ~= mt
end

F_flatten = function(xs, leaf)
    leaf = leaf or default_leaf
    local zs = {}
    local function f(ys)
        for i = 1, #ys do
            local x = ys[i]
            if type(x) == "table" and not leaf(x) then
                f(x)
            else
                zs[#zs+1] = x
            end
        end
    end
    f(xs)
    return setmetatable(zs, mt)
end
F.flatten = F_flatten
mt.__index.flatten = F_flatten

end

--[=[@@@
```lua
F.str({s1, s2, ... sn}, [separator, [last_separator]])
ss:str([separator, [last_separator]])
```
> concatenates strings (separated with an optional separator) and returns a string.
@@@]=]

function F.str(ss, sep, last_sep)
    if last_sep then
        if #ss <= 1 then return ss[1] or "" end
        return t_concat({t_concat(ss, sep, 1, #ss-1), ss[#ss]}, last_sep)
    else
        return t_concat(ss, sep)
    end
end
mt.__index.str = F.str

--[[@@@
```lua
F.from_set(f, ks)
ks:from_set(f)
```
> Build a map from a set of keys and a function which for each key computes its value.
@@@]]

F_from_set = function(f, ks)
    local t = {}
    for i = 1, #ks do
        local k = ks[i]
        t[k] = f(k)
    end
    return setmetatable(t, mt)
end
F.from_set = F_from_set
mt.__index.from_set = function(ks, f) return F_from_set(f, ks) end

--[[@@@
```lua
F.from_list(kvs)
kvs:from_list()
```
> Build a map from a list of key/value pairs.
@@@]]

function F.from_list(kvs)
    local t = {}
    for i = 1, #kvs do
        local k, v = t_unpack(kvs[i])
        t[k] = v
    end
    return setmetatable(t, mt)
end
mt.__index.from_list = F.from_list

--[[------------------------------------------------------------------------@@@
## Iterators
@@@]]

--[[@@@
```lua
F.pairs(t, [comp_lt])
t:pairs([comp_lt])
F.ipairs(xs, [comp_lt])
xs:ipairs([comp_lt])
```
> behave like the Lua `pairs` and `ipairs` iterators.
> `F.pairs` sorts keys using the function `comp_lt` or the default `<=` operator for keys (`F.op.klt`).
@@@]]

F.ipairs = ipairs
mt.__index.ipairs = ipairs

F_pairs = function(t, comp_lt)
    local ks = F_keys(t, comp_lt)
    local i = 0
    return function()
        i = i+1
        local k = ks[i]
        return k, t[k]
    end
end
F.pairs = F_pairs
mt.__index.pairs = F_pairs

--[[@@@
```lua
F.keys(t, [comp_lt])
t:keys([comp_lt])
F.values(t, [comp_lt])
t:values([comp_lt])
F.items(t, [comp_lt])
t:items([comp_lt])
```
> returns the list of keys, values or pairs of keys/values (same order than F.pairs).
@@@]]

F_keys = function(t, comp_lt)
    comp_lt = comp_lt or key_lt
    local ks = {}
    for k, _ in pairs(t) do ks[#ks+1] = k end
    t_sort(ks, comp_lt)
    return setmetatable(ks, mt)
end
F.keys = F_keys
mt.__index.keys = F_keys

function F.values(t, comp_lt)
    local ks = F_keys(t, comp_lt)
    local vs = t_create(#ks)
    for i = 1, #ks do vs[i] = t[ks[i]] end
    return setmetatable(vs, mt)
end
mt.__index.values = F.values

function F.items(t, comp_lt)
    local ks = F_keys(t, comp_lt)
    local kvs = t_create(#ks)
    for i = 1, #ks do
        local k = ks[i]
        kvs[i] = setmetatable({k, t[k]}, mt) end
    return setmetatable(kvs, mt)
end
mt.__index.items = F.items

--[[------------------------------------------------------------------------@@@
## Table extraction
@@@]]

--[[@@@
```lua
F.head(xs)
xs:head()
F.last(xs)
xs:last()
```
> returns the first element (head) or the last element (last) of a list.
@@@]]

F_head = function(xs) return xs[1] end
F.head = F_head
mt.__index.head = F.head

F_last = function(xs) return xs[#xs] end
F.last = F_last
mt.__index.last = F.last

--[[@@@
```lua
F.tail(xs)
xs:tail()
F.init(xs)
xs:init()
```
> returns the list after the head (tail) or before the last element (init).
@@@]]

F_tail = function(xs)
    local tail = t_move(xs, 2, #xs, 1, {})
    return setmetatable(tail, mt)
end
F.tail = F_tail
mt.__index.tail = F_tail

F_init = function(xs)
    local init = t_move(xs, 1, #xs-1, 1, {})
    return setmetatable(init, mt)
end
F.init = F_init
mt.__index.init = F_init

--[[@@@
```lua
F.uncons(xs)
xs:uncons()
```
> returns the head and the tail of a list.
@@@]]

function F.uncons(xs) return F_head(xs), F_tail(xs) end
mt.__index.uncons = F.uncons

--[[@@@
```lua
F.unpack(xs, [ i, [j] ])
xs:unpack([ i, [j] ])
```
> returns the elements of xs between indices i and j
@@@]]

F.unpack = table.unpack
mt.__index.unpack = table.unpack

--[[@@@
```lua
F.take(n, xs)
xs:take(n)
```
> Returns the prefix of xs of length n.
@@@]]

F_take = function(n, xs)
    local ys = t_move(xs, 1, n, 1, {})
    return setmetatable(ys, mt)
end
F.take = F_take
mt.__index.take = function(xs, n) return F_take(n, xs) end

--[[@@@
```lua
F.drop(n, xs)
xs:drop(n)
```
> Returns the suffix of xs after the first n elements.
@@@]]

F_drop = function(n, xs)
    local ys = t_move(xs, n+1, #xs, 1, {})
    return setmetatable(ys, mt)
end
F.drop = F_drop
mt.__index.drop = function(xs, n) return F_drop(n, xs) end

--[[@@@
```lua
F.split_at(n, xs)
xs:split_at(n)
```
> Returns a tuple where first element is xs prefix of length n and second element is the remainder of the list.
@@@]]

do

local function F_split_at(n, xs)
    return F_take(n, xs), F_drop(n, xs)
end
F.split_at = F_split_at
mt.__index.split_at = function(xs, n) return F_split_at(n, xs) end

end

--[[@@@
```lua
F.take_while(p, xs)
xs:take_while(p)
```
> Returns the longest prefix (possibly empty) of xs of elements that satisfy p.
@@@]]

F_take_while = function(p, xs)
    local i = 1
    while i <= #xs and p(xs[i]) do i = i+1 end
    local ys = t_move(xs, 1, i-1, 1, {})
    return setmetatable(ys, mt)
end
F.take_while = F_take_while
mt.__index.take_while = function(xs, p) return F_take_while(p, xs) end

--[[@@@
```lua
F.drop_while(p, xs)
xs:drop_while(p)
```
> Returns the suffix remaining after `take_while(p, xs)`{.lua}.
@@@]]

F_drop_while = function(p, xs)
    local i = 1
    while i <= #xs and p(xs[i]) do i = i+1 end
    local zs = t_move(xs, i, #xs, 1, {})
    return setmetatable(zs, mt)
end
F.drop_while = F_drop_while
mt.__index.drop_while = function(xs, p) return F_drop_while(p, xs) end

--[[@@@
```lua
F.drop_while_end(p, xs)
xs:drop_while_end(p)
```
> Drops the largest suffix of a list in which the given predicate holds for all elements.
@@@]]

F_drop_while_end = function(p, xs)
    local i = #xs
    while i > 0 and p(xs[i]) do i = i-1 end
    local zs = t_move(xs, 1, i, 1, {})
    return setmetatable(zs, mt)
end
F.drop_while_end = F_drop_while_end
mt.__index.drop_while_end = function(xs, p) return F_drop_while_end(p, xs) end

--[[@@@
```lua
F.span(p, xs)
xs:span(p)
```
> Returns a tuple where first element is longest prefix (possibly empty) of xs of elements that satisfy p and second element is the remainder of the list.
@@@]]

do

local function F_span(p, xs)
    local i = 1
    while i <= #xs and p(xs[i]) do i = i+1 end
    local ys = t_move(xs, 1, i-1, 1, {})
    local zs = t_move(xs, i, #xs, 1, {})
    return setmetatable(ys, mt), setmetatable(zs, mt)
end
F.span = F_span
mt.__index.span = function(xs, p) return F_span(p, xs) end

end

--[[@@@
```lua
F.break_(p, xs)
xs:break_(p)
```
> Returns a tuple where first element is longest prefix (possibly empty) of xs of elements that do not satisfy p and second element is the remainder of the list.
@@@]]

do

local function F_break(p, xs)
    local i = 1
    while i <= #xs and not p(xs[i]) do i = i+1 end
    local ys = t_move(xs, 1, i-1, 1, {})
    local zs = t_move(xs, i, #xs, 1, {})
    return setmetatable(ys, mt), setmetatable(zs, mt)
end
F.break_ = F_break
mt.__index.break_ = function(xs, p) return F_break(p, xs) end

end

--[[@@@
```lua
F.strip_prefix(prefix, xs)
xs:strip_prefix(prefix)
```
> Drops the given prefix from a list.
@@@]]

do

local function F_strip_prefix(prefix, xs)
    for i = 1, #prefix do
        if xs[i] ~= prefix[i] then return nil end
    end
    local ys = t_move(xs, #prefix+1, #xs, 1, {})
    return setmetatable(ys, mt)
end
F.strip_prefix = F_strip_prefix
mt.__index.strip_prefix = function(xs, prefix) return F_strip_prefix(prefix, xs) end

end

--[[@@@
```lua
F.strip_suffix(suffix, xs)
xs:strip_suffix(suffix)
```
> Drops the given suffix from a list.
@@@]]

do

local function F_strip_suffix(suffix, xs)
    for i = 1, #suffix do
        if xs[#xs-#suffix+i] ~= suffix[i] then return nil end
    end
    local ys = t_move(xs, 1, #xs-#suffix, 1, {})
    return setmetatable(ys, mt)
end
F.strip_suffix = F_strip_suffix
mt.__index.strip_suffix = function(xs, suffix) return F_strip_suffix(suffix, xs) end

end

--[[@@@
```lua
F.group(xs, [comp_eq])
xs:group([comp_eq])
```
> Returns a list of lists such that the concatenation of the result is equal to the argument. Moreover, each sublist in the result contains only equal elements.
@@@]]

function F.group(xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local yss = {}
    if #xs == 0 then return setmetatable(yss, mt) end
    local y = xs[1]
    local ys = {y}
    for i = 2, #xs do
        local x = xs[i]
        if comp_eq(x, y) then
            ys[#ys+1] = x
        else
            yss[#yss+1] = setmetatable(ys, mt)
            y = x
            ys = {y}
        end
    end
    yss[#yss+1] = setmetatable(ys, mt)
    return setmetatable(yss, mt)
end
mt.__index.group = F.group

--[[@@@
```lua
F.inits(xs)
xs:inits()
```
> Returns all initial segments of the argument, shortest first.
@@@]]

function F.inits(xs)
    local yss = t_create(#xs+1)
    for i = 0, #xs do
        local ys = t_move(xs, 1, i, 1, {})
        yss[#yss+1] = setmetatable(ys, mt)
    end
    return setmetatable(yss, mt)
end
mt.__index.inits = F.inits

--[[@@@
```lua
F.tails(xs)
xs:tails()
```
> Returns all final segments of the argument, longest first.
@@@]]

function F.tails(xs)
    local yss = t_create(#xs+1)
    for i = 1, #xs+1 do
        local ys = t_move(xs, i, #xs, 1, {})
        yss[#yss+1] = setmetatable(ys, mt)
    end
    return setmetatable(yss, mt)
end
mt.__index.tails = F.tails

--[[@@@
```lua
F.sub(xs, i, [j])
xs:sub(i, [j])
```
> Returns the sublist of `xs` that starts at `i` and continues until `j`.
> `i` and `j` can be negative (see `string.sub`).
@@@]]

function F.sub(xs, i, j)
    j = j or -1
    local n = #xs
    if i < 0 then i = n+1 + i end
    if j < 0 then j = n+1 + j end
    local ys = {}
    for k = i, j do
        ys[#ys+1] = xs[k]
    end
    return setmetatable(ys, mt)
end
mt.__index.sub = F.sub

--[[------------------------------------------------------------------------@@@
## Predicates
@@@]]

--[[@@@
```lua
F.is_prefix_of(prefix, xs)
prefix:is_prefix_of(xs)
```
> Returns `true` iff `xs` starts with `prefix`
@@@]]

F_is_prefix_of = function(prefix, xs)
    for i = 1, #prefix do
        if xs[i] ~= prefix[i] then return false end
    end
    return true
end
F.is_prefix_of = F_is_prefix_of
mt.__index.is_prefix_of = F_is_prefix_of

--[[@@@
```lua
F.is_suffix_of(suffix, xs)
suffix:is_suffix_of(xs)
```
> Returns `true` iff `xs` ends with `suffix`
@@@]]

F_is_suffix_of = function(suffix, xs)
    for i = 1, #suffix do
        if xs[#xs-#suffix+i] ~= suffix[i] then return false end
    end
    return true
end
F.is_suffix_of = F_is_suffix_of
mt.__index.is_suffix_of = F_is_suffix_of

--[[@@@
```lua
F.is_infix_of(infix, xs)
infix:is_infix_of(xs)
```
> Returns `true` iff `xs` contains `infix`
@@@]]

F_is_infix_of = function(infix, xs)
    for i = 1, #xs-#infix+1 do
        local found = true
        for j = 1, #infix do
            if xs[i+j-1] ~= infix[j] then found = false; break end
        end
        if found then return true end
    end
    return false
end
F.is_infix_of = F_is_infix_of
mt.__index.is_infix_of = F_is_infix_of

--[[@@@
```lua
F.has_prefix(xs, prefix)
xs:has_prefix(prefix)
```
> Returns `true` iff `xs` starts with `prefix`
@@@]]

F.has_prefix = function(xs, prefix) return F_is_prefix_of(prefix, xs) end
mt.__index.has_prefix = F.has_prefix

--[[@@@
```lua
F.has_suffix(xs, suffix)
xs:has_suffix(suffix)
```
> Returns `true` iff `xs` ends with `suffix`
@@@]]

F.has_suffix = function(xs, suffix) return F_is_suffix_of(suffix, xs) end
mt.__index.has_suffix = F.has_suffix

--[[@@@
```lua
F.has_infix(xs, infix)
xs:has_infix(infix)
```
> Returns `true` iff `xs` caontains `infix`
@@@]]

F.has_infix = function(xs, infix) return F_is_infix_of(infix, xs) end
mt.__index.has_infix = F.has_infix

--[[@@@
```lua
F.is_subsequence_of(seq, xs)
seq:is_subsequence_of(xs)
```
> Returns `true` if all the elements of the first list occur, in order, in the second. The elements do not have to occur consecutively.
@@@]]

function  F.is_subsequence_of(seq, xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local i = 1
    local j = 1
    while j <= #xs do
        if i > #seq then return true end
        if comp_eq(xs[j], seq[i]) then
            i = i+1
        end
        j = j+1
    end
    return false
end
mt.__index.is_subsequence_of = F.is_subsequence_of

--[[@@@
```lua
F.is_submap_of(t1, t2)
t1:is_submap_of(t2)
```
> returns true if all keys in t1 are in t2.
@@@]]

function F.is_submap_of(t1, t2)
    for k, _ in pairs(t1) do
        if t2[k] == nil then return false end
    end
    return true
end
mt.__index.is_submap_of = F.is_submap_of

--[[@@@
```lua
F.map_contains(t1, t2)
t1:map_contains(t2)
```
> returns true if all keys in t2 are in t1.
@@@]]

function F.map_contains(t1, t2)
    for k, _ in pairs(t2) do
        if t1[k] == nil then return false end
    end
    return true
end
mt.__index.map_contains = F.map_contains

--[[@@@
```lua
F.is_proper_submap_of(t1, t2)
t1:is_proper_submap_of(t2)
```
> returns true if all keys in t1 are in t2 and t1 keys and t2 keys are different.
@@@]]

function F.is_proper_submap_of(t1, t2)
    for k, _ in pairs(t1) do
        if t2[k] == nil then return false end
    end
    for k, _ in pairs(t2) do
        if t1[k] == nil then return true end
    end
    return false
end
mt.__index.is_proper_submap_of = F.is_proper_submap_of

--[[@@@
```lua
F.map_strictly_contains(t1, t2)
t1:map_strictly_contains(t2)
```
> returns true if all keys in t2 are in t1.
@@@]]

function F.map_strictly_contains(t1, t2)
    for k, _ in pairs(t2) do
        if t1[k] == nil then return false end
    end
    for k, _ in pairs(t1) do
        if t2[k] == nil then return true end
    end
    return false
end
mt.__index.map_strictly_contains = F.map_strictly_contains

--[[------------------------------------------------------------------------@@@
## Searching
@@@]]

--[[@@@
```lua
F.elem(x, xs, [comp_eq])
xs:elem(x, [comp_eq])
```
> Returns `true` if x occurs in xs (using the optional comp_eq function).
@@@]]

do

local F_elem = function(x, xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    for i = 1, #xs do
        if comp_eq(xs[i], x) then return true end
    end
    return false
end
F.elem = F_elem
mt.__index.elem = function(xs, x, comp_eq) return F_elem(x, xs, comp_eq) end

end

--[[@@@
```lua
F.not_elem(x, xs, [comp_eq])
xs:not_elem(x, [comp_eq])
```
> Returns `true` if x does not occur in xs (using the optional comp_eq function).
@@@]]

do

local function F_not_elem(x, xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    for i = 1, #xs do
        if comp_eq(xs[i], x) then return false end
    end
    return true
end
F.not_elem = F_not_elem
mt.__index.not_elem = function(xs, x, comp_eq) return F_not_elem(x, xs, comp_eq) end

end

--[[@@@
```lua
F.lookup(x, xys, [comp_eq])
xys:lookup(x, [comp_eq])
```
> Looks up a key `x` in an association list (using the optional comp_eq function).
@@@]]

do

local function F_lookup(x, xys, comp_eq)
    comp_eq = comp_eq or F_op_eq
    for i = 1, #xys do
        if comp_eq(xys[i][1], x) then return xys[i][2] end
    end
    return nil
end
F.lookup = F_lookup
mt.__index.lookup = function(xys, x, comp_eq) return F_lookup(x, xys, comp_eq) end

end

--[[@@@
```lua
F.find(p, xs)
xs:find(p)
```
> Returns the leftmost element of xs matching the predicate p.
@@@]]

do

local function F_find(p, xs)
    for i = 1, #xs do
        local x = xs[i]
        if p(x) then return x end
    end
    return nil
end
F.find = F_find
mt.__index.find = function(xs, p) return F_find(p, xs) end

end

--[[@@@
```lua
F.filter(p, xs)
xs:filter(p)
```
> Returns the list of those elements that satisfy the predicate p(x).
@@@]]

local function F_filter(p, xs)
    local ys = {}
    for i = 1, #xs do
        local x = xs[i]
        if p(x) then ys[#ys+1] = x end
    end
    return setmetatable(ys, mt)
end
F.filter = F_filter
mt.__index.filter = function(xs, p) return F_filter(p, xs) end

--[[@@@
```lua
F.filter_eq(y, xs)
xs:filter_eq(y)
F.filter_ne(y, xs)
xs:filter_ne(y)
F.filter_lt(y, xs)
xs:filter_lt(y)
F.filter_le(y, xs)
xs:filter_le(y)
F.filter_gt(y, xs)
xs:filter_gt(y)
F.filter_ge(y, xs)
xs:filter_ge(y)
```
> Returns the list of those elements that satisfy the predicate
  F.op.eq(x, y), f.op.ne(x, y), f.op.lt(x, y), f.op.le(x, y), f.op.gt(x, y), f.op.ge(x, y).
@@@]]

--[[@@@
```lua
F.filter_ueq(y, xs)
xs:filter_ueq(y)
F.filter_une(y, xs)
xs:filter_une(y)
F.filter_ult(y, xs)
xs:filter_ult(y)
F.filter_ule(y, xs)
xs:filter_ule(y)
F.filter_ugt(y, xs)
xs:filter_ugt(y)
F.filter_uge(y, xs)
xs:filter_uge(y)
```
> Returns the list of those elements that satisfy the predicate
  F.op.ueq(x, y), f.op.une(x, y), f.op.ult(x, y), f.op.ule(x, y), f.op.ugt(x, y), f.op.uge(x, y).
@@@]]

--[[@@@
```lua
F.filter_keq(y, xs)
xs:filter_keq(y)
F.filter_kne(y, xs)
xs:filter_kne(y)
F.filter_klt(y, xs)
xs:filter_klt(y)
F.filter_kle(y, xs)
xs:filter_kle(y)
F.filter_kgt(y, xs)
xs:filter_kgt(y)
F.filter_kge(y, xs)
xs:filter_kge(y)
```
> Returns the list of those elements that satisfy the predicate
  F.op.keq(x, y), f.op.kne(x, y), f.op.klt(x, y), f.op.kle(x, y), f.op.kgt(x, y), f.op.kge(x, y).
@@@]]

for _, op in ipairs{"eq", "ne", "lt", "le", "gt", "ge"} do
    for _, t in ipairs{"", "u", "k"} do
        local f = F.op[t..op]
        F["filter_"..t..op]          = function(y, xs) return F_filter(function(x) return f(x, y) end, xs) end
        mt.__index["filter_"..t..op] = function(xs, y) return F_filter(function(x) return f(x, y) end, xs) end
    end
end

--[[@@@
```lua
F.filteri(p, xs)
xs:filteri(p)
```
> Returns the list of those elements that satisfy the predicate p(i, x).
@@@]]

do

local function F_filteri(p, xs)
    local ys = {}
    for i = 1, #xs do
        local x = xs[i]
        if p(i, x) then ys[#ys+1] = x end
    end
    return setmetatable(ys, mt)
end
F.filteri = F_filteri
mt.__index.filteri = function(xs, p) return F_filteri(p, xs) end

end

--[[@@@
```lua
F.filter2t(p, xs)
xs:filter2t(p)
```
> filters the elements of `xs` with `p` and returns the table `{p(xs[1])=xs[1], p(xs[2])=xs[2], ...}`
> where `p(x)` is a predicate that returns the key for `x` in the returned table (`nil` to reject `x`).
@@@]]

do

local function F_filter2t(p, xs)
    local t = {}
    for i = 1, #xs do
        local v = xs[i]
        local k = p(v)
        if k~=nil then rawset(t, k, v) end
    end
    return setmetatable(t, mt)
end
F.filter2t = F_filter2t
mt.__index.filter2t = function(xs, p) return F_filter2t(p, xs) end

end

--[[@@@
```lua
F.filteri2t(p, xs)
xs:filteri2t(p)
```
> filters the elements of `xs` with `p` and returns the table `{p(1, xs[1])=xs[1], p(2, xs[2])=xs[2], ...}`
> where `p(i, x)` is a predicate that returns the key for `x` in the returned table (`nil` to reject `x`).
@@@]]

do

local function F_filteri2t(p, xs)
    local t = {}
    for i = 1, #xs do
        local v = xs[i]
        local k = p(i, v)
        if k~=nil then rawset(t, k, v) end
    end
    return setmetatable(t, mt)
end
F.filteri2t = F_filteri2t
mt.__index.filteri2t = function(xs, p) return F_filteri2t(p, xs) end

end

--[[@@@
```lua
F.filtert(p, t)
t:filtert(p)
```
> Returns the table of those values that satisfy the predicate p(v).
@@@]]

do

local function F_filtert(p, t)
    local t2 = {}
    for k, v in pairs(t) do
        if p(v) then t2[k] = v end
    end
    return setmetatable(t2, mt)
end
F.filtert = F_filtert
mt.__index.filtert = function(t, p) return F_filtert(p, t) end

end

--[[@@@
```lua
F.filterk(p, t)
t:filterk(p)
```
> Returns the table of those values that satisfy the predicate p(k, v).
@@@]]

F_filterk = function(p, t)
    local t2 = {}
    for k, v in pairs(t) do
        if p(k, v) then t2[k] = v end
    end
    return setmetatable(t2, mt)
end
F.filterk = F_filterk
mt.__index.filterk = function(t, p) return F_filterk(p, t) end

-- filtert2a
-- filterk2a

--[[@@@
```lua
F.filtert2a(p, t)
t:filtert2a(p)
```
> filters `t` with `p` and returns the array `{t[k1], t[k2], ...}` for all `t[ki]` that satisfy `p(t[ki])`.
@@@]]

do

local function F_filtert2a(p, t)
    local ys = {}
    for _, v in F_pairs(t) do
        if p(v) then ys[#ys+1] = v end
    end
    return setmetatable(ys, mt)
end
F.filtert2a = F_filtert2a
mt.__index.filtert2a = function(t, p) return F_filtert2a(p, t) end

end

--[[@@@
```lua
F.filterk2a(f, t)
t:filterk2a(f)
```
> filters `t` with `p` and returns the array `{t[k1], t[k2], ...}` for all `t[ki]` that satisfy `p(ki, t[ki])`.
@@@]]

do

local function F_filterk2a(p, t)
    local ys = {}
    for k, v in F_pairs(t) do
        if p(k, v) then ys[#ys+1] = v end
    end
    return setmetatable(ys, mt)
end
F.filterk2a = F_filterk2a
mt.__index.filterk2a = function(t, p) return F_filterk2a(p, t) end

end

--[[@@@
```lua
F.restrict_keys(t, ks)
t:restrict_keys(ks)
```
> Restrict a map to only those keys found in a list.
@@@]]

function F.restrict_keys(t, ks)
    local kset = F_from_set(F_const(true), ks)
    local function p(k, _) return kset[k] end
    return F_filterk(p, t)
end
mt.__index.restrict_keys = F.restrict_keys

--[[@@@
```lua
F.without_keys(t, ks)
t:without_keys(ks)
```
> Remove all keys in a list from a map.
@@@]]

function F.without_keys(t, ks)
    local kset = F_from_set(F_const(true), ks)
    local function p(k, _) return not kset[k] end
    return F_filterk(p, t)
end
mt.__index.without_keys = F.without_keys

--[[@@@
```lua
F.partition(p, xs)
xs:partition(p)
```
> Returns the pair of lists of elements which do and do not satisfy the predicate, respectively.
@@@]]

do

local function F_partition(p, xs)
    local ys = {}
    local zs = {}
    for i = 1, #xs do
        local x = xs[i]
        if p(x) then ys[#ys+1] = x else zs[#zs+1] = x end
    end
    return setmetatable(ys, mt), setmetatable(zs, mt)
end
F.partition = F_partition
mt.__index.partition = function(xs, p) return F_partition(p, xs) end

end

--[[@@@
```lua
F.table_partition(p, t)
t:table_partition(p)
```
> Partition the map according to a predicate. The first map contains all elements that satisfy the predicate, the second all elements that fail the predicate.
@@@]]

do

local function F_table_partition(p, t)
    local t1, t2 = {}, {}
    for k, v in pairs(t) do
        if p(v) then t1[k] = v else t2[k] = v end
    end
    return setmetatable(t1, mt), setmetatable(t2, mt)
end
F.table_partition = F_table_partition
mt.__index.table_partition = function(t, p) return F_table_partition(p, t) end

end

--[[@@@
```lua
F.table_partition_with_key(p, t)
t:table_partition_with_key(p)
```
> Partition the map according to a predicate. The first map contains all elements that satisfy the predicate, the second all elements that fail the predicate.
@@@]]

do

local function F_table_partition_with_key(p, t)
    local t1, t2 = {}, {}
    for k, v in pairs(t) do
        if p(k, v) then t1[k] = v else t2[k] = v end
    end
    return setmetatable(t1, mt), setmetatable(t2, mt)
end
F.table_partition_with_key = F_table_partition_with_key
mt.__index.table_partition_with_key = function(t, p) return F_table_partition_with_key(p, t) end

end

--[[@@@
```lua
F.elem_index(x, xs)
xs:elem_index(x)
```
> Returns the index of the first element in the given list which is equal to the query element.
@@@]]

do

local function F_elem_index(x, xs)
    for i = 1, #xs do
        if x == xs[i] then return i end
    end
    return nil
end
F.elem_index = F_elem_index
mt.__index.elem_index = function(xs, x) return F_elem_index(x, xs) end

end

--[[@@@
```lua
F.elem_indices(x, xs)
xs:elem_indices(x)
```
> Returns the indices of all elements equal to the query element, in ascending order.
@@@]]

do

local function F_elem_indices(x, xs)
    local indices = {}
    for i = 1, #xs do
        if x == xs[i] then indices[#indices+1] = i end
    end
    return setmetatable(indices, mt)
end
F.elem_indices = F_elem_indices
mt.__index.elem_indices = function(xs, x) return F_elem_indices(x, xs) end

end

--[[@@@
```lua
F.find_index(p, xs)
xs:find_index(p)
```
> Returns the index of the first element in the list satisfying the predicate.
@@@]]

do

local function F_find_index(p, xs)
    for i = 1, #xs do
        if p(xs[i]) then return i end
    end
    return nil
end
F.find_index = F_find_index
mt.__index.find_index = function(xs, p) return F_find_index(p, xs) end

end

--[[@@@
```lua
F.find_indices(p, xs)
xs:find_indices(p)
```
> Returns the indices of all elements satisfying the predicate, in ascending order.
@@@]]

do

local function F_find_indices(p, xs)
    local indices = {}
    for i = 1, #xs do
        if p(xs[i]) then indices[#indices+1] = i end
    end
    return setmetatable(indices, mt)
end
F.find_indices = F_find_indices
mt.__index.find_indices = function(xs, p) return F_find_indices(p, xs) end

end

--[[------------------------------------------------------------------------@@@
## Table size
@@@]]

--[[@@@
```lua
F.null(xs)
xs:null()
F.null(t)
t:null("t")
```
> checks wether a list or a table is empty.
@@@]]

F_null = function(t)
    return next(t) == nil
end
F.null = F_null
mt.__index.null = F_null

--[[@@@
```lua
#xs
F.length(xs)
xs:length()
```
> Length of a list.
@@@]]

F_length = function(xs)
    return #xs
end
F.length = F_length
mt.__index.length = F_length

--[[@@@
```lua
F.size(t)
t:size()
```
> Size of a table (number of (key, value) pairs).
@@@]]

function F.size(t)
    local n = 0
    for _, _ in pairs(t) do
        n = n+1
    end
    return n
end
mt.__index.size = F.size

--[[------------------------------------------------------------------------@@@
## Table transformations
@@@]]

--[[@@@
```lua
F.map(f, xs)
xs:map(f)
```
> maps `f` to the elements of `xs` and returns `{f(xs[1]), f(xs[2]), ...}`
> (`nil` values are ignored)
@@@]]

F_map = function(f, xs)
    local ys = t_create(#xs)
    for i = 1, #xs do ys[#ys+1] = f(xs[i]) end
    return setmetatable(ys, mt)
end
F.map = F_map
mt.__index.map = function(xs, f) return F_map(f, xs) end

--[[@@@
```lua
F.mapi(f, xs)
xs:mapi(f)
```
> maps `f` to the indices and elements of `xs` and returns `{f(1, xs[1]), f(2, xs[2]), ...}`
> (`nil` values are ignored)
@@@]]

do

local function F_mapi(f, xs)
    local ys = t_create(#xs)
    for i = 1, #xs do ys[#ys+1] = f(i, xs[i]) end
    return setmetatable(ys, mt)
end
F.mapi = F_mapi
mt.__index.mapi = function(xs, f) return F_mapi(f, xs) end

end

--[[@@@
```lua
F.map2t(f, xs)
xs:map2t(f)
```
> maps `f` to the elements of `xs` and returns the table `{fk(xs[1])=fv(xs[1]), fk(xs[2])=fv(xs[2]), ...}`
> where `f(x)` returns two values `fk(x), fv(x)` used as the keys and values of the returned table.
> (`nil` values are ignored)
@@@]]

do

local function F_map2t(f, xs)
    local t = {}
    for i = 1, #xs do
        local k, v = f(xs[i])
        if k~=nil then rawset(t, k, v) end
    end
    return setmetatable(t, mt)
end
F.map2t = F_map2t
mt.__index.map2t = function(xs, f) return F_map2t(f, xs) end

end

--[[@@@
```lua
F.mapi2t(f, xs)
xs:mapi2t(f)
```
> maps `f` to the indices and elements of `xs` and returns `{fk(1, xs[1])=fv(1, xs[1]), fk(2, xs[2])=fv(2, xs[2]), ...}`
> where `f(i, x)` returns two values `fk(i, x), fv(i, x)` used as the keys and values of the returned table.
> (`nil` values are ignored)
@@@]]

do

local function F_mapi2t(f, xs)
    local t = {}
    for i = 1, #xs do
        local k, v = f(i, xs[i])
        if k~=nil then rawset(t, k, v) end
    end
    return setmetatable(t, mt)
end
F.mapi2t = F_mapi2t
mt.__index.mapi2t = function(xs, f) return F_mapi2t(f, xs) end

end

--[[@@@
```lua
F.mapt(f, t)
t:mapt(f)
```
> maps `f` to the values of `t` and returns `{k1=f(t[k1]), k2=f(t[k2]), ...}`
> (`nil` values are ignored)
@@@]]

do

local function F_mapt(f, t)
    local t2 = {}
    for k, v in pairs(t) do t2[k] = f(v) end
    return setmetatable(t2, mt)
end
F.mapt = F_mapt
mt.__index.mapt = function(t, f) return F_mapt(f, t) end

end

--[[@@@
```lua
F.mapk(f, t)
t:mapk(f)
```
> maps `f` to the keys and values of `t` and returns `{k1=f(k1, t[k1]), k2=f(k2, t[k2]), ...}`
> (`nil` values are ignored)
@@@]]

do

local function F_mapk(f, t)
    local t2 = {}
    for k, v in pairs(t) do t2[k] = f(k, v) end
    return setmetatable(t2, mt)
end
F.mapk = F_mapk
mt.__index.mapk = function(t, f) return F_mapk(f, t) end

end

--[[@@@
```lua
F.mapt2a(f, t)
t:mapt2a(f)
```
> maps `f` to the values of `t` and returns the array `{f(t[k1]), f(t[k2]), ...}`
> (`nil` values are ignored)
@@@]]

do

local function F_mapt2a(f, t)
    local ys = {}
    for _, v in F_pairs(t) do ys[#ys+1] = f(v) end
    return setmetatable(ys, mt)
end
F.mapt2a = F_mapt2a
mt.__index.mapt2a = function(t, f) return F_mapt2a(f, t) end

end

--[[@@@
```lua
F.mapk2a(f, t)
t:mapk2a(f)
```
> maps `f` to the keys and the values of `t` and returns the array `{f(k1, t[k1]), f(k2, t[k2]), ...}`
> (`nil` values are ignored)
@@@]]

do

local function F_mapk2a(f, t)
    local ys = {}
    for k, v in F_pairs(t) do ys[#ys+1] = f(k, v) end
    return setmetatable(ys, mt)
end
F.mapk2a = F_mapk2a
mt.__index.mapk2a = function(t, f) return F_mapk2a(f, t) end

end

--[[@@@
```lua
F.reverse(xs)
xs:reverse()
```
> reverses the order of a list
@@@]]

function F.reverse(xs)
    local ys = t_create(#xs)
    for i = #xs, 1, -1 do ys[#ys+1] = xs[i] end
    return setmetatable(ys, mt)
end
mt.__index.reverse = F.reverse

--[[@@@
```lua
F.transpose(xss)
xss:transpose()
```
> Transposes the rows and columns of its argument.
@@@]]

function F.transpose(xss)
    local N = #xss
    local M = max(t_unpack(F_map(F_length, xss)))
    local yss = t_create(M)
    for j = 1, M do
        local ys = t_create(N)
        for i = 1, N do ys[i] = xss[i][j] end
        yss[j] = setmetatable(ys, mt)
    end
    return setmetatable(yss, mt)
end
mt.__index.transpose = F.transpose

--[[@@@
```lua
F.update(f, k, t)
t:update(f, k)
```
> Updates the value `x` at `k`. If `f(x)` is nil, the element is deleted. Otherwise the key `k` is bound to the value `f(x)`.
>
> **Warning**: in-place modification.
@@@]]

do

local function F_update(f, k, t)
    t[k] = f(t[k])
    return t
end
F.update = F_update
mt.__index.update = function(t, f, k) return F_update(f, k, t) end

end

--[[@@@
```lua
F.updatek(f, k, t)
t:updatek(f, k)
```
> Updates the value `x` at `k`. If `f(k, x)` is nil, the element is deleted. Otherwise the key `k` is bound to the value `f(k, x)`.
>
> **Warning**: in-place modification.
@@@]]

do

local function F_updatek(f, k, t)
    t[k] = f(k, t[k])
    return t
end
F.updatek = F_updatek
mt.__index.updatek = function(t, f, k) return F_updatek(f, k, t) end

end

--[[------------------------------------------------------------------------@@@
## Table transversal
@@@]]

--[[@@@
```lua
F.foreach(xs, f)
xs:foreach(f)
```
> calls `f` with the elements of `xs` (`f(xi)` for `xi` in `xs`)
@@@]]

F_foreach = function(xs, f)
    for i = 1, #xs do f(xs[i]) end
end
F.foreach = F_foreach
mt.__index.foreach = F_foreach

--[[@@@
```lua
F.foreachi(xs, f)
xs:foreachi(f)
```
> calls `f` with the indices and elements of `xs` (`f(i, xi)` for `xi` in `xs`)
@@@]]

function F.foreachi(xs, f)
    for i = 1, #xs do f(i, xs[i]) end
end
mt.__index.foreachi = F.foreachi

--[[@@@
```lua
F.foreacht(t, f)
t:foreacht(f)
```
> calls `f` with the values of `t` (`f(v)` for `v` in `t` such that `v = t[k]`)
@@@]]

function F.foreacht(t, f)
    for _, v in F_pairs(t) do f(v) end
end
mt.__index.foreacht = F.foreacht

--[[@@@
```lua
F.foreachk(t, f)
t:foreachk(f)
```
> calls `f` with the keys and values of `t` (`f(k, v)` for (`k`, `v`) in `t` such that `v = t[k]`)
@@@]]

function F.foreachk(t, f)
    for k, v in F_pairs(t) do f(k, v) end
end
mt.__index.foreachk = F.foreachk

--[[------------------------------------------------------------------------@@@
## Table reductions (folds)
@@@]]

--[[@@@
```lua
F.fold(f, x, xs)
xs:fold(f, x)
```
> Left-associative fold of a list (`f(...f(f(x, xs[1]), xs[2]), ...)`).
@@@]]

do

local function F_fold(fzx, z, xs)
    for i = 1, #xs do
        z = fzx(z, xs[i])
    end
    return z
end
F.fold = F_fold
mt.__index.fold = function(xs, fzx, z) return F_fold(fzx, z, xs) end

end

--[[@@@
```lua
F.foldi(f, x, xs)
xs:foldi(f, x)
```
> Left-associative fold of a list (`f(...f(f(x, 1, xs[1]), 2, xs[2]), ...)`).
@@@]]

do

local function F_foldi(fzx, z, xs)
    for i = 1, #xs do
        z = fzx(z, i, xs[i])
    end
    return z
end
F.foldi = F_foldi
mt.__index.foldi = function(xs, fzx, z) return F_foldi(fzx, z, xs) end

end

--[[@@@
```lua
F.fold1(f, xs)
xs:fold1(f)
```
> Left-associative fold of a list, the initial value is `xs[1]`.
@@@]]

do

local function F_fold1(fzx, xs)
    local z = xs[1]
    for i = 2, #xs do
        z = fzx(z, xs[i])
    end
    return z
end
F.fold1 = F_fold1
mt.__index.fold1 = function(xs, fzx) return F_fold1(fzx, xs) end

end

--[[@@@
```lua
F.foldt(f, x, t)
t:foldt(f, x)
```
> Left-associative fold of a table (in the order given by F.pairs).
@@@]]

do

local function F_foldt(fzx, z, t)
    for _, v in F_pairs(t) do
        z = fzx(z, v)
    end
    return z
end
F.foldt = F_foldt
mt.__index.foldt = function(t, fzx, z) return F_foldt(fzx, z, t) end

end

--[[@@@
```lua
F.foldk(f, x, t)
t:foldk(f, x)
```
> Left-associative fold of a table (in the order given by F.pairs).
@@@]]

do

local function F_foldk(fzx, z, t)
    for k, v in F_pairs(t) do
        z = fzx(z, k, v)
    end
    return z
end
F.foldk = F_foldk
mt.__index.foldk = function(t, fzx, z) return F_foldk(fzx, z, t) end

end

--[[@@@
```lua
F.land(bs)
bs:land()
```
> Returns the conjunction of a container of booleans.
@@@]]

function F.land(bs)
    for i = 1, #bs do if not bs[i] then return false end end
    return true
end
mt.__index.land = F.land

--[[@@@
```lua
F.lor(bs)
bs:lor()
```
> Returns the disjunction of a container of booleans.
@@@]]

function F.lor(bs)
    for i = 1, #bs do if bs[i] then return true end end
    return false
end
mt.__index.lor = F.lor

--[[@@@
```lua
F.any(p, xs)
xs:any(p)
```
> Determines whether any element of the structure satisfies the predicate.
@@@]]

do

local function F_any(p, xs)
    for i = 1, #xs do if p(xs[i]) then return true end end
    return false
end
F.any = F_any
mt.__index.any = function(xs, p) return F_any(p, xs) end

end

--[[@@@
```lua
F.all(p, xs)
xs:all(p)
```
> Determines whether all elements of the structure satisfy the predicate.
@@@]]

do

local function F_all(p, xs)
    for i = 1, #xs do if not p(xs[i]) then return false end end
    return true
end
F.all = F_all
mt.__index.all = function(xs, p) return F_all(p, xs) end

end

--[[@@@
```lua
F.sum(xs)
xs:sum()
```
> Returns the sum of the numbers of a structure.
@@@]]

function F.sum(xs)
    local s = 0
    for i = 1, #xs do s = s + xs[i] end
    return s
end
mt.__index.sum = F.sum

--[[@@@
```lua
F.product(xs)
xs:product()
```
> Returns the product of the numbers of a structure.
@@@]]

function F.product(xs)
    local p = 1
    for i = 1, #xs do p = p * xs[i] end
    return p
end
mt.__index.product = F.product

--[[@@@
```lua
F.maximum(xs, [comp_lt])
xs:maximum([comp_lt])
```
> The largest element of a non-empty structure, according to the optional comparison function.
@@@]]

function F.maximum(xs, comp_lt)
    if #xs == 0 then return nil end
    comp_lt = comp_lt or F_op_lt
    local xmax = xs[1]
    for i = 2, #xs do
        if not comp_lt(xs[i], xmax) then xmax = xs[i] end
    end
    return xmax
end
mt.__index.maximum = F.maximum

--[[@@@
```lua
F.minimum(xs, [comp_lt])
xs:minimum([comp_lt])
```
> The least element of a non-empty structure, according to the optional comparison function.
@@@]]

F_minimum = function(xs, comp_lt)
    if #xs == 0 then return nil end
    comp_lt = comp_lt or F_op_lt
    local min = xs[1]
    for i = 2, #xs do
        if comp_lt(xs[i], min) then min = xs[i] end
    end
    return min
end
F.minimum = F_minimum
mt.__index.minimum = F_minimum

--[[@@@
```lua
F.scan(f, x, xs)
xs:scan(f, x)
```
> Similar to `fold` but returns a list of successive reduced values from the left.
@@@]]

do

local function F_scan(fzx, z, xs)
    local zs = {z}
    for i = 1, #xs do
        z = fzx(z, xs[i])
        zs[#zs+1] = z
    end
    return setmetatable(zs, mt)
end
F.scan = F_scan
mt.__index.scan = function(xs, fzx, z) return F_scan(fzx, z, xs) end

end

--[[@@@
```lua
F.scan1(f, xs)
xs:scan1(f)
```
> Like `scan` but the initial value is `xs[1]`.
@@@]]

do

local function F_scan1(fzx, xs)
    local z = xs[1]
    local zs = {z}
    for i = 2, #xs do
        z = fzx(z, xs[i])
        zs[#zs+1] = z
    end
    return setmetatable(zs, mt)
end
F.scan1 = F_scan1
mt.__index.scan1 = function(xs, fzx) return F_scan1(fzx, xs) end

end

--[[@@@
```lua
F.concat_map(f, xs)
xs:concat_map(f)
```
> Map a function over all the elements of a container and concatenate the resulting lists.
@@@]]

do

local function F_concat_map(fx, xs)
    return F_concat(F_map(fx, xs))
end
F.concat_map = F_concat_map
mt.__index.concat_map = function(xs, fx) return F_concat_map(fx, xs) end

end

--[[------------------------------------------------------------------------@@@
## Zipping
@@@]]

--[[@@@
```lua
F.zip(xss, [f])
xss:zip([f])
```
> `zip` takes a list of lists and returns a list of corresponding tuples.
@@@]]

F_zip = function(xss, f)
    local ns = F_minimum(F_map(F_length, xss))
    local yss = t_create(ns)
    if f then
        for i = 1, ns do
            local ys = F_map(function(xs) return xs[i] end, xss)
            yss[i] = f(t_unpack(ys))
        end
    else
        for i = 1, ns do
            local ys = F_map(function(xs) return xs[i] end, xss)
            yss[i] = ys
        end
    end
    return setmetatable(yss, mt)
end
F.zip = F_zip
mt.__index.zip = F_zip

--[[@@@
```lua
F.unzip(xss)
xss:unzip()
```
> Transforms a list of n-tuples into n lists
@@@]]

function F.unzip(xss)
    return t_unpack(F_zip(xss))
end
mt.__index.unzip = F.unzip

--[[@@@
```lua
F.zip_with(f, xss)
xss:zip_with(f)
```
> `zip_with` generalises `zip` by zipping with the function given as the first argument, instead of a tupling function.
@@@]]

function F.zip_with(f, xss) return F_zip(xss, f) end
mt.__index.zip_with = F_zip

--[[------------------------------------------------------------------------@@@
## Cartesian product
@@@]]

--[[@@@
```lua
F.cross(xss, [f])
xss:cross([f])
```
> `cross` takes a list of lists and returns the Cartesian product of these lists.
@@@]]

local function F_cross(xss, f)
    f = f or function(...) return setmetatable({...}, mt) end
    local ts = t_create(F_map(F.op.len, xss):product())
    local function rec(i, t)
        if i <= #xss then
            local xs = xss[i]
            for j = 1, #xs do
                t[i] = xs[j]
                rec(i+1, t)
            end
        else
            ts[#ts+1] = f(t_unpack(t))
        end
    end
    rec(1, t_create(#xss))
    return setmetatable(ts, mt)
end
F.cross = F_cross
mt.__index.cross = F_cross

--[[@@@
```lua
F.cross_with(f, xss)
xss:cross_with(f)
```
> `cross_with` generalises `cross` by crossing with the function given as the first argument, instead of a tupling function.
@@@]]

function F.cross_with(f, xss) return F_cross(xss, f) end
mt.__index.cross_with = F_cross

--[[------------------------------------------------------------------------@@@
## Set operations
@@@]]

--[[@@@
```lua
F.nub(xs, [comp_eq])
xs:nub([comp_eq])
```
> Removes duplicate elements from a list. In particular, it keeps only the first occurrence of each element, according to the optional comp_eq function.
@@@]]

function F.nub(xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local ys = t_create(#xs)
    for i = 1, #xs do
        local x = xs[i]
        local found = false
        for j = 1, #ys do
            if comp_eq(x, ys[j]) then found = true; break end
        end
        if not found then ys[#ys+1] = x end
    end
    return setmetatable(ys, mt)
end
mt.__index.nub = F.nub

--[[@@@
```lua
F.delete(x, xs, [comp_eq])
xs:delete(x, [comp_eq])
```
> Removes the first occurrence of x from its list argument, according to the optional comp_eq function.
@@@]]

do

local function F_delete(x, xs, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local ys = t_create(#xs)
    local i = 1
    while i <= #xs and not comp_eq(xs[i], x) do i = i+1 end
    t_move(xs, 1, i-1, 1, ys)
    t_move(xs, i+1, #xs, #ys+1, ys)
    return setmetatable(ys, mt)
end
F.delete = F_delete
mt.__index.delete = function(xs, x, comp_eq) return F_delete(x, xs, comp_eq) end

end

--[[@@@
```lua
F.difference(xs, ys, [comp_eq])
xs:difference(ys, [comp_eq])
```
> Returns the list difference. In `difference(xs, ys)`{.lua} the first occurrence of each element of ys in turn (if any) has been removed from xs, according to the optional comp_eq function.
@@@]]

function F.difference(xs, ys, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local zs = t_create(#xs)
    ys = {t_unpack(ys)}
    for i = 1, #xs do
        local x = xs[i]
        local found = false
        for j = 1, #ys do
            if comp_eq(ys[j], x) then
                found = true
                t_remove(ys, j)
                break
            end
        end
        if not found then zs[#zs+1] = x end
    end
    return setmetatable(zs, mt)
end
mt.__index.difference = F.difference

--[[@@@
```lua
F.union(xs, ys, [comp_eq])
xs:union(ys, [comp_eq])
```
> Returns the list union of the two lists. Duplicates, and elements of the first list, are removed from the the second list, but if the first list contains duplicates, so will the result, according to the optional comp_eq function.
@@@]]

function F.union(xs, ys, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local zs = {t_unpack(xs)}
    for i = 1, #ys do
        local y = ys[i]
        local found = false
        for j = 1, #zs do
            if comp_eq(y, zs[j]) then found = true; break end
        end
        if not found then zs[#zs+1] = y end
    end
    return setmetatable(zs, mt)
end
mt.__index.union = F.union

--[[@@@
```lua
F.intersection(xs, ys, [comp_eq])
xs:intersection(ys, [comp_eq])
```
> Returns the list intersection of two lists. If the first list contains duplicates, so will the result, according to the optional comp_eq function.
@@@]]

function F.intersection(xs, ys, comp_eq)
    comp_eq = comp_eq or F_op_eq
    local zs = {}
    for i = 1, #xs do
        local x = xs[i]
        local found = false
        for j = 1, #ys do
            if comp_eq(x, ys[j]) then found = true; break end
        end
        if found then zs[#zs+1] = x end
    end
    return setmetatable(zs, mt)
end
mt.__index.intersection = F.intersection

--[[------------------------------------------------------------------------@@@
## Table operations
@@@]]

--[[@@@
```lua
F.merge(ts)
ts:merge()
F.table_union(ts)
ts:table_union()
```
> Right-biased union of tables.
@@@]]

F_merge = function(ts)
    local u = {}
    for i = 1, #ts do
        for k, v in pairs(ts[i]) do u[k] = v end
    end
    return setmetatable(u, mt)
end
F.merge = F_merge
mt.__index.merge = F_merge

F.table_union = F_merge
mt.__index.table_union = F_merge

--[[@@@
```lua
F.merge_with(f, ts)
ts:merge_with(f)
F.table_union_with(f, ts)
ts:table_union_with(f)
```
> Right-biased union of tables with a combining function.
@@@]]

do

local function F_merge_with(f, ts)
    local u = {}
    for i = 1, #ts do
        for k, v in pairs(ts[i]) do
            local uk = u[k]
            if uk == nil then
                u[k] = v
            else
                u[k] = f(u[k], v)
            end
        end
    end
    return setmetatable(u, mt)
end
F.merge_with = F_merge_with
mt.__index.merge_with = function(ts, f) return F_merge_with(f, ts) end

F.table_union_with = F_merge_with
mt.__index.table_union_with = mt.__index.merge_with

end

--[[@@@
```lua
F.merge_with_key(f, ts)
ts:merge_with_key(f)
F.table_union_with_key(f, ts)
ts:table_union_with_key(f)
```
> Right-biased union of tables with a combining function.
@@@]]

do

local function F_merge_with_key(f, ts)
    local u = {}
    for i = 1, #ts do
        for k, v in pairs(ts[i]) do
            local uk = u[k]
            if uk == nil then
                u[k] = v
            else
                u[k] = f(k, u[k], v)
            end
        end
    end
    return setmetatable(u, mt)
end
F.merge_with_key = F_merge_with_key
mt.__index.merge_with_key = function(ts, f) return F_merge_with_key(f, ts) end

F.table_union_with_key = F_merge_with_key
mt.__index.table_union_with_key = mt.__index.merge_with_key

end

--[[@@@
```lua
F.table_difference(t1, t2)
t1:table_difference(t2)
```
> Difference of two maps. Return elements of the first map not existing in the second map.
@@@]]

function F.table_difference(t1, t2)
    local t = {}
    for k, v in pairs(t1) do if t2[k] == nil then t[k] = v end end
    return setmetatable(t, mt)
end
mt.__index.table_difference = F.table_difference

--[[@@@
```lua
F.table_difference_with(f, t1, t2)
t1:table_difference_with(f, t2)
```
> Difference with a combining function. When two equal keys are encountered, the combining function is applied to the values of these keys.
@@@]]

do

local function F_table_difference_with(f, t1, t2)
    local t = {}
    for k, v1 in pairs(t1) do
        local v2 = t2[k]
        if v2 == nil then
            t[k] = v1
        else
            t[k] = f(v1, v2)
        end
    end
    return setmetatable(t, mt)
end
F.table_difference_with = F_table_difference_with
mt.__index.table_difference_with = function(t1, f, t2) return F_table_difference_with(f, t1, t2) end

end

--[[@@@
```lua
F.table_difference_with_key(f, t1, t2)
t1:table_difference_with_key(f, t2)
```
> Union with a combining function.
@@@]]

do

local function F_table_difference_with_key(f, t1, t2)
    local t = {}
    for k, v1 in pairs(t1) do
        local v2 = t2[k]
        if v2 == nil then
            t[k] = v1
        else
            t[k] = f(k, v1, v2)
        end
    end
    return setmetatable(t, mt)
end
F.table_difference_with_key = F_table_difference_with_key
mt.__index.table_difference_with_key = function(t1, f, t2) return F_table_difference_with_key(f, t1, t2) end

end

--[[@@@
```lua
F.table_intersection(t1, t2)
t1:table_intersection(t2)
```
> Intersection of two maps. Return data in the first map for the keys existing in both maps.
@@@]]

function F.table_intersection(t1, t2)
    local t = {}
    for k, v in pairs(t1) do if t2[k] ~= nil then t[k] = v end end
    return setmetatable(t, mt)
end
mt.__index.table_intersection = F.table_intersection

--[[@@@
```lua
F.table_intersection_with(f, t1, t2)
t1:table_intersection_with(f, t2)
```
> Difference with a combining function. When two equal keys are encountered, the combining function is applied to the values of these keys.
@@@]]

do

local function F_table_intersection_with(f, t1, t2)
    local t = {}
    for k, v1 in pairs(t1) do
        local v2 = t2[k]
        if v2 ~= nil then
            t[k] = f(v1, v2)
        end
    end
    return setmetatable(t, mt)
end
F.table_intersection_with = F_table_intersection_with
mt.__index.table_intersection_with = function(t1, f, t2) return F_table_intersection_with(f, t1, t2) end

end

--[[@@@
```lua
F.table_intersection_with_key(f, t1, t2)
t1:table_intersection_with_key(f, t2)
```
> Union with a combining function.
@@@]]

do

local function F_table_intersection_with_key(f, t1, t2)
    local t = {}
    for k, v1 in pairs(t1) do
        local v2 = t2[k]
        if v2 ~= nil then
            t[k] = f(k, v1, v2)
        end
    end
    return setmetatable(t, mt)
end
F.table_intersection_with_key = F_table_intersection_with_key
mt.__index.table_intersection_with_key = function(t1, f, t2) return F_table_intersection_with_key(f, t1, t2) end

end

--[[@@@
```lua
F.disjoint(t1, t2)
t1:disjoint(t2)
```
> Check the intersection of two maps is empty.
@@@]]

function F.disjoint(t1, t2)
    for k, _ in pairs(t1) do if t2[k] ~= nil then return false end end
    return true
end
mt.__index.disjoint = F.disjoint

--[[@@@
```lua
F.table_compose(t1, t2)
t1:table_compose(t2)
```
> Relate the keys of one map to the values of the other, by using the values of the former as keys for lookups in the latter.
@@@]]

function F.table_compose(t1, t2)
    local t = {}
    for k2, v2 in pairs(t2) do
        local v1 = t1[v2]
        t[k2] = v1
    end
    return setmetatable(t, mt)
end
mt.__index.table_compose = F.table_compose

--[[@@@
```lua
F.Nil
```
> `F.Nil` is a singleton used to represent `nil` (see `F.patch`)
@@@]]
Nil = setmetatable({}, {
    __name = "Nil",
    __call = function(_) return nil end,
    __tostring = function(_) return "Nil" end,
})
F.Nil = Nil

--[[@@@
```lua
F.patch(t1, t2)
t1:patch(t2)
```
> returns a copy of `t1` where some fields are replaced by values from `t2`.
Keys not found in `t2` are not modified.
If `t2` contains `F.Nil` then the corresponding key is removed from `t1`.
Unmodified subtrees are not cloned but returned as is (common subtrees are shared).
@@@]]

do

local function F_patch(t1, t2)
    if t2 == nil then return t1 end -- value not patched
    if t2 == Nil then return nil end -- remove t1
    if type(t1) ~= "table" then return t2 end -- replace a scalar field by a scalar or a table
    if type(t2) ~= "table" then return t2 end -- a scalar replaces a scalar or a table
    local t = {}
    -- patch fields from t1 with values from t2
    for k, v1 in pairs(t1) do
        local v2 = t2[k]
        t[k] = F_patch(v1, v2)
    end
    -- add new values from t2
    for k, v2 in pairs(t2) do
        local v1 = t1[k]
        if v1 == nil then
            t[k] = v2
        end
    end
    return setmetatable(t, mt)
end

F.patch = F_patch
mt.__index.patch = F_patch

end

--[[------------------------------------------------------------------------@@@
## Ordered lists
@@@]]

--[[@@@
```lua
F.sort(xs, [comp_lt])
xs:sort([comp_lt])
```
> Sorts xs from lowest to highest, according to the optional comp_lt function.
@@@]]

function F.sort(xs, comp_lt)
    local ys = t_move(xs, 1, #xs, 1, {})
    t_sort(ys, comp_lt)
    return setmetatable(ys, mt)
end
mt.__index.sort = F.sort

--[[@@@
```lua
F.sort_on(f, xs, [comp_lt])
xs:sort_on(f, [comp_lt])
```
> Sorts a list by comparing the results of a key function applied to each element, according to the optional comp_lt function.
@@@]]

do

local function F_sort_on(f, xs, comp_lt)
    comp_lt = comp_lt or F_op_lt
    local ys = t_create(#xs)
    for i = 1, #xs do ys[i] = {f(xs[i]), xs[i]} end
    t_sort(ys, function(a, b) return comp_lt(a[1], b[1]) end)
    local zs = t_create(#xs)
    for i = 1, #ys do zs[i] = ys[i][2] end
    return setmetatable(zs, mt)
end
F.sort_on = F_sort_on
mt.__index.sort_on = function(xs, f, comp_lt) return F_sort_on(f, xs, comp_lt) end

end

--[[@@@
```lua
F.insert(x, xs, [comp_lt])
xs:insert(x, [comp_lt])
```
> Inserts the element into the list at the first position where it is less than or equal to the next element, according to the optional comp_lt function.
@@@]]

do

local function F_insert(x, xs, comp_lt)
    comp_lt = comp_lt or F_op_lt
    local ys = t_create(#xs+1)
    local i = 1
    while i <= #xs and not comp_lt(x, xs[i]) do i = i+1 end
    t_move(xs, 1, i-1, 1, ys)
    ys[#ys+1] = x
    t_move(xs, i, #xs, #ys+1, ys)
    return setmetatable(ys, mt)
end
F.insert = F_insert
mt.__index.insert = function(xs, x, comp_lt) return F_insert(x, xs, comp_lt) end

end

--[[------------------------------------------------------------------------@@@
## Miscellaneous functions
@@@]]

--[[@@@
```lua
F.subsequences(xs)
xs:subsequences()
```
> Returns the list of all subsequences of the argument.
@@@]]

do

local function F_subsequences(xs)
    if F_null(xs) then return setmetatable({{}}, mt) end
    local inits = F_subsequences(F_init(xs))
    local last = F_last(xs)
    return inits .. F_map(function(seq) return F_concat{seq, {last}} end, inits)
end
F.subsequences = F_subsequences
mt.__index.subsequences = F_subsequences

end

--[[@@@
```lua
F.permutations(xs)
xs:permutations()
```
> Returns the list of all permutations of the argument.
@@@]]

local function fact(n)
    local p = 1
    for i = 2, n do p = p * i end
    return p
end

F_permutations = function(xs)
    local perms = t_create(fact(#xs))
    local n = #xs
    xs = t_move(xs, 1, #xs, 1, {})
    local function permute(k)
        if k > n then perms[#perms+1] = t_move(xs, 1, #xs, 1, {})
        else
            for i = k, n do
                xs[k], xs[i] = xs[i], xs[k]
                permute(k+1)
                xs[k], xs[i] = xs[i], xs[k]
            end
        end
    end
    permute(1)
    return setmetatable(perms, mt)
end
F.permutations = F_permutations
mt.__index.permutations = F_permutations

--[[------------------------------------------------------------------------@@@
## Functions on strings
@@@]]

--[[@@@
```lua
string.chars(s, i, j)
s:chars(i, j)
```
> Returns the list of characters of a string between indices i and j, or the whole string if i and j are not provided.
@@@]]

s_chars = function(s, i, j)
    return F_map(s_char, s_bytes(s, i, j))
end
string.chars = s_chars

--[[@@@
```lua
string.bytes(s, i, j)
s:bytes(i, j)
```
> Returns the list of byte codes of a string between indices i and j, or the whole string if i and j are not provided.
@@@]]

s_bytes = function(s, i, j)
    return setmetatable({s_byte(s, i or 1, j or -1)}, mt)
end
string.bytes = s_bytes

--[[@@@
```lua
string.head(s)
s:head()
```
> Extract the first element of a string.
@@@]]

s_head = function(s)
    if #s == 0 then return nil end
    return s_sub(s, 1, 1)
end
string.head = s_head

--[[@@@
```lua
sting.last(s)
s:last()
```
> Extract the last element of a string.
@@@]]

s_last = function(s)
    if #s == 0 then return nil end
    return s_sub(s, -1)
end
string.last = s_last

--[[@@@
```lua
string.tail(s)
s:tail()
```
> Extract the elements after the head of a string
@@@]]

s_tail = function(s)
    if #s == 0 then return nil end
    return s_sub(s, 2)
end
string.tail = s_tail

--[[@@@
```lua
string.init(s)
s:init()
```
> Return all the elements of a string except the last one.
@@@]]

s_init = function(s)
    if #s == 0 then return nil end
    return s_sub(s, 1, -2)
end
string.init = s_init

--[[@@@
```lua
string.uncons(s)
s:uncons()
```
> Decompose a string into its head and tail.
@@@]]

function string.uncons(s)
    return s_head(s), s_tail(s)
end

--[[@@@
```lua
string.null(s)
s:null()
```
> Test whether the string is empty.
@@@]]

function string.null(s)
    return #s == 0
end

--[[@@@
```lua
string.length(s)
s:length()
```
> Returns the length of a string.
@@@]]

function string.length(s)
    return #s
end

--[[@@@
```lua
string.intersperse(c, s)
c:intersperse(s)
```
> Intersperses a element c between the elements of s.
@@@]]

function string.intersperse(c, s)
    if #s < 2 then return s end
    local cs = t_create(#s*2)
    for i = 1, #s-1 do
        cs[#cs+1] = s_sub(s, i, i)
        cs[#cs+1] = c
    end
    cs[#cs+1] = s_sub(s, -1)
    return t_concat(cs)
end

--[[@@@
```lua
string.intercalate(s, ss)
s:intercalate(ss)
```
> Inserts the string s in between the strings in ss and concatenates the result.
@@@]]

function string.intercalate(s, ss)
    return t_concat(ss, s)
end

--[[@@@
```lua
string.subsequences(s)
s:subsequences()
```
> Returns the list of all subsequences of the argument.
@@@]]

local function s_subsequences(s)
    if s=="" then return {""} end
    local inits = s_subsequences(s_init(s))
    local last = s_last(s)
    return inits .. F_map(function(seq) return seq..last end, inits)
end
string.subsequences = s_subsequences

--[[@@@
```lua
string.permutations(s)
s:permutations()
```
> Returns the list of all permutations of the argument.
@@@]]

function string.permutations(s)
    return F_map(t_concat, F_permutations(s_chars(s)))
end

--[[@@@
```lua
string.take(s, n)
s:take(n)
```
> Returns the prefix of s of length n.
@@@]]

s_take = function(s, n)
    if n <= 0 then return "" end
    return s_sub(s, 1, n)
end
string.take = s_take

--[[@@@
```lua
string.drop(s, n)
s:drop(n)
```
> Returns the suffix of s after the first n elements.
@@@]]

s_drop = function(s, n)
    if n <= 0 then return s end
    return s_sub(s, n+1)
end
string.drop = s_drop

--[[@@@
```lua
string.split_at(s, n)
s:split_at(n)
```
> Returns a tuple where first element is s prefix of length n and second element is the remainder of the string.
@@@]]

function string.split_at(s, n)
    return s_take(s, n), s_drop(s, n)
end

--[[@@@
```lua
string.take_while(s, p)
s:take_while(p)
```
> Returns the longest prefix (possibly empty) of s of elements that satisfy p.
@@@]]

function string.take_while(s, p)
    return t_concat(F_take_while(p, s_chars(s)))
end

--[[@@@
```lua
string.drop_while(s, p)
s:drop_while(p)
```
> Returns the suffix remaining after `s:take_while(p)`{.lua}.
@@@]]

function string.drop_while(s, p)
    return t_concat(F_drop_while(p, s_chars(s)))
end

--[[@@@
```lua
string.drop_while_end(s, p)
s:drop_while_end(p)
```
> Drops the largest suffix of a string in which the given predicate holds for all elements.
@@@]]

function string.drop_while_end(s, p)
    return t_concat(F_drop_while_end(p, s_chars(s)))
end

--[[@@@
```lua
string.strip_prefix(s, prefix)
s:strip_prefix(prefix)
```
> Drops the given prefix from a string.
@@@]]

function string.strip_prefix(s, prefix)
    local n = #prefix
    if s_sub(s, 1, n) == prefix then return s_sub(s, n+1) end
    return nil
end

--[[@@@
```lua
string.strip_suffix(s, suffix)
s:strip_suffix(suffix)
```
> Drops the given suffix from a string.
@@@]]

function string.strip_suffix(s, suffix)
    local n = #suffix
    if s_sub(s, #s-n+1) == suffix then return s_sub(s, 1, #s-n) end
    return nil
end

--[[@@@
```lua
string.inits(s)
s:inits()
```
> Returns all initial segments of the argument, shortest first.
@@@]]

function string.inits(s)
    local ss = t_create(#s+1)
    for i = 0, #s do
        ss[#ss+1] = s_sub(s, 1, i)
    end
    return setmetatable(ss, mt)
end

--[[@@@
```lua
string.tails(s)
s:tails()
```
> Returns all final segments of the argument, longest first.
@@@]]

function string.tails(s)
    local ss = t_create(#s+1)
    for i = 1, #s+1 do
        ss[#ss+1] = s_sub(s, i)
    end
    return setmetatable(ss, mt)
end

--[[@@@
```lua
string.is_prefix_of(prefix, s)
prefix:is_prefix_of(s)
```
> Returns `true` iff the first string is a prefix of the second.
@@@]]

function string.is_prefix_of(prefix, s)
    return s_sub(s, 1, #prefix) == prefix
end

--[[@@@
```lua
string.has_prefix(s, prefix)
s:has_prefix(prefix)
```
> Returns `true` iff the second string is a prefix of the first.
@@@]]

function string.has_prefix(s, prefix)
    return s_sub(s, 1, #prefix) == prefix
end

--[[@@@
```lua
string.is_suffix_of(suffix, s)
suffix:is_suffix_of(s)
```
> Returns `true` iff the first string is a suffix of the second.
@@@]]

function string.is_suffix_of(suffix, s)
    return s_sub(s, #s-#suffix+1) == suffix
end

--[[@@@
```lua
string.has_suffix(s, suffix)
s:has_suffix(suffix)
```
> Returns `true` iff the second string is a suffix of the first.
@@@]]

function string.has_suffix(s, suffix)
    return s_sub(s, #s-#suffix+1) == suffix
end

--[[@@@
```lua
string.is_infix_of(infix, s)
infix:is_infix_of(s)
```
> Returns `true` iff the first string is contained, wholly and intact, anywhere within the second.
@@@]]

function string.is_infix_of(infix, s)
    return s_find(s, infix) ~= nil
end

--[[@@@
```lua
string.has_infix(s, infix)
s:has_infix(infix)
```
> Returns `true` iff the second string is contained, wholly and intact, anywhere within the first.
@@@]]

function string.has_infix(s, infix)
    return s_find(s, infix) ~= nil
end

--[[@@@
```lua
string.matches(s, pattern, [init])
s:matches(pattern, [init])
```
> Returns the list of the captures from `pattern` by iterating on `string.gmatch`.
  If `pattern` defines two or more captures, the result is a list of list of captures.
@@@]]

function string.matches(s, pattern, init)
    local iterator = s_gmatch(s, pattern, init)
    local ms = setmetatable({}, mt)
    while true do
        local xs = {iterator()}
        if #xs == 0 then return ms end
        ms[#ms+1] = #xs==1 and xs[1] or setmetatable(xs, mt)
    end
end

--[[@@@
```lua
string.split(s, sep, maxsplit, plain)
s:split(sep, maxsplit, plain)
```
> Splits a string `s` around the separator `sep`. `maxsplit` is the maximal number of separators. If `plain` is true then the separator is a plain string instead of a Lua string pattern.
@@@]]

s_split = function(s, sep, maxsplit, plain)
    assert(sep and sep ~= "")
    maxsplit = maxsplit or (1/0)
    local items = {}
    if #s > 0 then
        local init = 1
        for _ = 1, maxsplit do
            local m, n = s_find(s, sep, init, plain)
            if m and m <= n then
                t_insert(items, s_sub(s, init, m-1))
                init = n + 1
            else
                break
            end
        end
        t_insert(items, s_sub(s, init))
    end
    return setmetatable(items, mt)
end
string.split = s_split

--[[@@@
```lua
string.lines(s)
s:lines()
```
> Splits the argument into a list of lines stripped of their terminating `\n` characters.
@@@]]

function string.lines(s)
    local lines = s_split(s, '\r?\n\r?')
    if lines[#lines] == "" and s_match(s, '\r?\n\r?$') then t_remove(lines) end
    return setmetatable(lines, mt)
end

--[[@@@
```lua
string.words(s)
s:words()
```
> Breaks a string up into a list of words, which were delimited by white space.
@@@]]

function string.words(s)
    local words = s_split(s, '%s+')
    if words[1] == "" and s_match(s, '^%s+') then t_remove(words, 1) end
    if words[#words] == "" and s_match(s, '%s+$') then t_remove(words) end
    return setmetatable(words, mt)
end

--[[@@@
```lua
F.unlines(xs)
xs:unlines()
```
> Appends a `\n` character to each input string, then concatenates the results.
@@@]]

function F.unlines(xs)
    local s = t_create(#xs*2)
    for i = 1, #xs do
        s[#s+1] = xs[i]
        s[#s+1] = "\n"
    end
    return t_concat(s)
end
mt.__index.unlines = F.unlines

--[[@@@
```lua
string.unwords(xs)
xs:unwords()
```
> Joins words with separating spaces.
@@@]]

function F.unwords(xs)
    return t_concat(xs, " ")
end
mt.__index.unwords = F.unwords

--[[@@@
```lua
string.ltrim(s)
s:ltrim()
```
> Removes heading spaces
@@@]]

function string.ltrim(s)
    return (s_match(s, "^%s*(.*)"))
end

--[[@@@
```lua
string.rtrim(s)
s:rtrim()
```
> Removes trailing spaces
@@@]]

function string.rtrim(s)
    return (s_match(s, "(.-)%s*$"))
end

--[[@@@
```lua
string.trim(s)
s:trim()
```
> Removes heading and trailing spaces
@@@]]

function string.trim(s)
    return (s_match(s, "^%s*(.-)%s*$"))
end

--[[@@@
```lua
string.ljust(s, w, [c])
s:ljust(w)
```
> Left-justify `s` by appending spaces (or the character `c`).
  The result is at least `w` byte long. `s` is not truncated.
@@@]]

function string.ljust(s, w, c)
    return s .. s_rep(c or " ", w-#s)
end

--[[@@@
```lua
string.rjust(s, w, [c])
s:rjust(w)
```
> Right-justify `s` by prepending spaces (or the character `c`).
  The result is at least `w` byte long. `s` is not truncated.
@@@]]

function string.rjust(s, w, c)
    return s_rep(c or " ", w-#s) .. s
end

--[[@@@
```lua
string.center(s, w)
s:center(w)
```
> Center `s` by appending and prepending spaces (or the character `c`).
  The result is at least `w` byte long. `s` is not truncated.
@@@]]

function string.center(s, w, c)
    c = c or " "
    local l = (w-#s)//2
    local r = (w-#s)-l
    return s_rep(c, l) .. s .. s_rep(c, r)
end

--[[@@@
```lua
string.cap(s)
s:cap()
```
> Capitalizes a string. The first character is upper case, other are lower case.
@@@]]

s_cap = function(s)
    return s_upper(s_sub(s, 1, 1)) .. s_lower(s_sub(s, 2))
end
string.cap = s_cap

--[[------------------------------------------------------------------------@@@
## Identifier formatting
@@@]]

local function split_identifier(...)
    local words = {}
    local function add_word(name)
        -- an upper case letter starts a new word
        name = s_gsub(tostring(name), "([^%u])(%u)", "%1_%2")
        -- split words
        for w in s_gmatch(name, "%w+") do words[#words+1] = w end
    end
    F_foreach(F_flatten{...}, add_word)
    return setmetatable(words, mt)
end

--[[@@@
```lua
string.lower_snake_case(s)              -- e.g.: hello_world
string.upper_snake_case(s)              -- e.g.: HELLO_WORLD
string.lower_camel_case(s)              -- e.g.: helloWorld
string.upper_camel_case(s)              -- e.g.: HelloWorld
string.dotted_lower_snake_case(s)       -- e.g.: hello.world
string.dotted_upper_snake_case(s)       -- e.g.: HELLO.WORLD

F.lower_snake_case(s)                   -- e.g.: hello_world
F.upper_snake_case(s)                   -- e.g.: HELLO_WORLD
F.lower_camel_case(s)                   -- e.g.: helloWorld
F.upper_camel_case(s)                   -- e.g.: HelloWorld
F.dotted_lower_snake_case(s)            -- e.g.: hello.world
F.dotted_upper_snake_case(s)            -- e.g.: HELLO.WORLD

s:lower_snake_case()                    -- e.g.: hello_world
s:upper_snake_case()                    -- e.g.: HELLO_WORLD
s:lower_camel_case()                    -- e.g.: helloWorld
s:upper_camel_case()                    -- e.g.: HelloWorld
s:dotted_lower_snake_case()             -- e.g.: hello.world
s:dotted_upper_snake_case()             -- e.g.: HELLO.WORLD
```
> Convert an identifier using some wellknown naming conventions.
  `s` can be a string or a list of strings.
@@@]]

function string.lower_snake_case(...)
    return t_concat(F_map(s_lower, split_identifier(...)), "_")
end

function string.upper_snake_case(...)
    return t_concat(F_map(s_upper, split_identifier(...)), "_")
end

function string.lower_camel_case(...)
    return (s_gsub(t_concat(F_map(s_cap, split_identifier(...))), "^%w", s_lower))
end

function string.upper_camel_case(...)
    return t_concat(F_map(s_cap, split_identifier(...)))
end

function string.dotted_lower_snake_case(...)
    return t_concat(F_map(s_lower, split_identifier(...)), ".")
end

function string.dotted_upper_snake_case(...)
    return t_concat(F_map(s_upper, split_identifier(...)), ".")
end

F.lower_snake_case = string.lower_snake_case
mt.__index.lower_snake_case = string.lower_snake_case

F.upper_snake_case = string.upper_snake_case
mt.__index.upper_snake_case = string.upper_snake_case

F.lower_camel_case = string.lower_camel_case
mt.__index.lower_camel_case = string.lower_camel_case

F.upper_camel_case = string.upper_camel_case
mt.__index.upper_camel_case = string.upper_camel_case

F.dotted_lower_snake_case = string.dotted_lower_snake_case
mt.__index.dotted_lower_snake_case = string.dotted_lower_snake_case

F.dotted_upper_snake_case = string.dotted_upper_snake_case
mt.__index.dotted_upper_snake_case = string.dotted_upper_snake_case

--[[------------------------------------------------------------------------@@@
## String evaluation
@@@]]

--[[@@@
```lua
string.read(s)
s:read()
```
> Convert s to a Lua value (like `F.read`)
@@@]]

string.read = F.read

--[[------------------------------------------------------------------------@@@
## String interpolation
@@@]]

--[[@@@
```lua
F.I(t)
```
> returns a string interpolator that replaces `$(...)` with
> the value of `...` in the environment defined by the table `t`.
> An interpolator can be given another table
> to build a new interpolator with new values.
>
> The mod operator (`%`) produces a new interpolator with a custom pattern.
> E.g. `F.I % "@[]"` is an interpolator that replaces `@[...]` with
> the value of `...`.
@@@]]

do

local interpolator_mt = {}

function interpolator_mt:__mod(pattern)
    assert(type(pattern)=="string" and #pattern>=3)
    return setmetatable({
        pattern = s_gsub(pattern, "^(.+)(.)(.)$", "%1(%%b%2%3)"),
        env = self.env,
    }, interpolator_mt)
end

local function chain(self, new_env)
    return setmetatable({
        pattern = self.pattern,
        env = setmetatable({}, {
            __index = function(_, k)
                local v = new_env[k]
                if v ~= nil then return v end
                return self.env and self.env[k]
            end
        })
    }, interpolator_mt)
end

local function interpolate(self, s)
    return (s_gsub(s, self.pattern, function(x)
        local y = ((assert(load("return "..s_sub(x, 2, -2), nil, "t", self.env)))())
        if type(y) == "table" or type(y) == "userdata" then
            y = tostring(y)
        end
        return y
    end))
end

function interpolator_mt:__call(s)
    if type(s) == "string" then return interpolate(self, s) end
    if type(s) == "table" then return chain(self, s) end
    error "An interpolator expects a table or a string"
end

F.I = setmetatable({env=nil}, interpolator_mt) % "%$()"

end

--[[------------------------------------------------------------------------@@@
## Random array access

@@@]]

--[[@@@
```lua
F.choose(xs, prng)
F.choose(xs)    -- using the global PRNG
xs:choose(prng)
xs:choose()     -- using the global PRNG
```
> returns a random item from `xs`
@@@]]

function F.choose(xs, prng)
    if prng then
        return prng:choose(xs)
    else
        return require "crypt".choose(xs)
    end
end
mt.__index.choose = F.choose

--[[@@@
```lua
F.shuffle(xs, prng)
F.shuffle(xs)   -- using the global PRNG
xs:shuffle(prng)
xs:shuffle()    -- using the global PRNG
```
> returns a shuffled copy of `xs`
@@@]]

function F.shuffle(xs, prng)
    if prng then
        return prng:shuffle(xs)
    else
        return require "crypt".shuffle(xs)
    end
end
mt.__index.shuffle = F.shuffle

--[[------------------------------------------------------------------------@@@
## Schema-based table validation

This function validates the content of a table agains a schema.

A schema is a Lua table with the same structure that the input tables,
the values being replaced with values of the expected type.

| Expected type     | Specification                                                     | Exemple                   |
| ----------------- | ----------------------------------------------------------------- | ------------------------- |
| Boolean           | any boolean                                                       | `true`                    |
| Number            | any number                                                        | `0`                       |
| String            | any string                                                        | `"str"`                   |
| Array             | a table with one element (type of the array items)                | `{ "str" }`               |
| Structure         | a table with keys (names) and values (types)                      | `{ x=0, y=0 }`            |
| Enumerated type   | a list with the `"enum"` keyword and the list of values           | `{ "enum", "on", "off" }` |
| Interval          | a list with the `"range"` keyword and the min and max values      | `{ "range", -10, 10 }`    |
| Union             | a list with the `"union"` keyword and the list of accepted types  | `{ "union", 0, "str" }`   |
| Option            | a list with the `"option"` keyword and the optional type          | `{ "option", "str" }`     |
| Any value         | a list with the `"any"` keyword                                   | `{ "any" }`               |

The optional types can be combined with other types. E.g.: `{ "option", "range", -10, 10 }`.

@@@]]

--[[@@@
```lua
F.validate(schema, input, [options])
```
> returns `true` if `input` is validated by `schema`. Otherwise it returns `false`
> and a list of failures.
>
> | Options           | Description                                               | Default value |
> | ----------------- | --------------------------------------------------------- | ------------- |
> | `options.strict`  | Strict validation (unspecified fields are not allowed)    | `true`        |
@@@]]

function F.validate(schema, input, options)

    options = options or {}
    local strict = options.strict == nil or options.strict

    local function mkpath(path, k)
        if type(k) == "string" and k:match "^[%w_]+$" then
            return (path and (path..".") or "")..k
        end
        k = ("[%q]"):format(k)
        return (path or "")..k
    end

    local function q(x)
        return ("%q"):format(x)
    end

    local function walk(t, v, path, errs)

        -- detect missing fields (if not optional)
        if v == nil then
            if type(t) ~= "table" or t[1] ~= "option" then
                errs[#errs+1] = ("%s: missing field"):format(path)
            end
            return
        end

        if type(t) == "table" and t[1] == "option" then
            t = F.tail(t)
            if #t == 1 then t = t[1] end ---@diagnostic disable-line: need-check-nil
        end

        -- check atomic types (booleans, numbers and strings)
        if type(t) ~= "table" then
            if type(v) ~= type(t) then
                errs[#errs+1] = ("%s: %s expected"):format(path, type(t))
            end
            return
        end

        -- check any type
        if t[1] == "any" then
            return
        end

        -- check ranges
        if t[1] == "range" then
            local vmin, vmax = t[2], t[3]
            if type(vmin) ~= type(vmax) then
                errs[#errs+1] = ("%s: range type mismatch [%s, %s]"):format(path, type(vmin), type(vmax))
            elseif type(v) ~= type(vmin) then
                errs[#errs+1] = ("%s: %s expected"):format(path, type(vmin))
            elseif v < vmin or v > vmax then
                errs[#errs+1] = ("%s: shall be in [%q, %q]"):format(path, vmin, vmax)
            end
            return
        end

        -- check enumerations
        if t[1] == "enum" then
            local vs = assert(F.tail(t))
            for _, vi in pairs(vs) do
                if v == vi then return end
            end
            errs[#errs+1] = ("%s: shall be %s"):format(path, F.str(F.map(q, vs), ", ", " or "))
            return
        end

        -- check unions
        if t[1] == "union" then
            local ts = assert(F.tail(t))
            for _, ti in pairs(ts) do
                local l_errs = {}
                walk(ti, v, path, l_errs)
                if #l_errs == 0 then return end
            end
            errs[#errs+1] = ("%s: shall be %s"):format(path, F.show(ts))
            return
        end

        -- check array
        if t[1] ~= nil then
            if type(v) ~= "table" then
                errs[#errs+1] = ("%s: shall be an array"):format(path)
                return
            end
            for i, vi in ipairs(v) do
                walk(t[1], vi, mkpath(path, i), errs)
            end
            return
        end

        -- check table
        if type(v) ~= "table" then
            errs[#errs+1] = ("%s: shall be a table"):format(path)
            return
        end
        if strict then
            for ki in F.pairs(v) do
                if rawget(t, ki) == nil then
                    errs[#errs+1] = ("%s: unexpected field"):format(mkpath(path, ki))
                end
            end
        end
        for ki, ti in F.pairs(t) do
            walk(ti, rawget(v, ki), mkpath(path, ki), errs)
        end

    end

    local errs = F{}
    walk(schema, input, nil, errs)
    return #errs == 0, errs

end

-------------------------------------------------------------------------------
-- module
-------------------------------------------------------------------------------

local reg = debug.getregistry()
reg.luax_F_mt = mt

return setmetatable(F, {
    __call = function(_, t)
        if type(t) == "table" then return setmetatable(t, mt) end
        return t
    end,
})
