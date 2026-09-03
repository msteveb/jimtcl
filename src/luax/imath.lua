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

local has_imath, imath = pcall(require, "_imath")

if not has_imath then

    imath = {}

    local mt = {__index={}}

    local F = require "F"

    ---@diagnostic disable:unused-vararg
    local function ni(f) return function(...) error(f.." not implemented") end end

    local floor = math.floor
    local log = math.log
    local max = math.max
    local mtype = math.type

    local concat = table.concat

    local RADIX <const> = 10000000
    local RADIX_LEN <const> = floor(log(RADIX, 10))

    assert(RADIX^2 < 2^53, "RADIX^2 shall be storable on a Lua number")

    local int_add, int_sub, int_mul, int_absdivmod, int_divmod, int_shortdiv, int_abs

    local function int_trim(a)
        for i = #a, 1, -1 do
            if a[i] and a[i] ~= 0 then break end
            a[i] = nil
        end
        if #a == 0 then a.s = 1 end
    end

    local function int(n, base)
        n = n or 0
        if type(n) == "table" then return n end
        if type(n) == "number" then
            if mtype(n) == "float" then
                n = ("%.0f"):format(floor(n))
            else
                n = ("%d"):format(n)
            end
        end
        assert(type(n) == "string")
        n = n:gsub("[ _]", "")
        local sign = 1
        local d = 1 -- current digit index
        if n:sub(d, d) == "+" then d = d+1
        elseif n:sub(d, d) == "-" then sign = -1; d = d+1
        end
        if n:sub(d, d+1) == "0x" then d = d+2; base = 16
        elseif n:sub(d, d+1) == "0o" then d = d+2; base = 8
        elseif n:sub(d, d+1) == "0b" then d = d+2; base = 2
        else base = base or 10
        end
        local self = {s=1}
        if base == 10 then
            for i = #n, d, -RADIX_LEN do
                local digit = n:sub(max(d, i-RADIX_LEN+1), i)
                self[#self+1] = tonumber(digit)
            end
        else
            local bn_base = {s=1, base}
            local bn_shift = {s=1, 1}
            local bn_digit = {s=1, 0}
            for i = #n, d, -1 do
                bn_digit[1] = tonumber(n:sub(i, i), base)
                self = int_add(self, int_mul(bn_digit, bn_shift))
                bn_shift = int_mul(bn_shift, bn_base)
            end
        end
        self.s = sign
        int_trim(self)
        return setmetatable(self, mt)
    end

    local int_zero <const> = int(0)
    local int_one <const> = int(1)
    local int_two <const> = int(2)

    local function int_copy(n)
        local c = {s=n.s}
        for i = 1, #n do c[i] = n[i] end
        return setmetatable(c, mt)
    end

    local function int_tonumber(n)
        local s = {(n.s < 0 and #n > 0) and "-0" or "0"}
        local fmt = ("%%0%dd"):format(RADIX_LEN)
        for i = #n, 1, -1 do
            s[#s+1] = fmt:format(n[i])
        end
        s[#s+1] = "."
        return tonumber(concat(s))
    end

    local function int_tostring(n, base)
        base = base or 10
        local s = F{}
        local sign = n.s
        if base == 10 then
            local fmt = ("%%0%dd"):format(RADIX_LEN)
            for i = 1, #n do
                s[#s+1] = fmt:format(n[i])
            end
            s = concat(s:reverse())
            s = s:gsub("^[_0]+", "")
            if s == "" then s = "0" end
        else
            local absn = int_abs(n)
            while #absn > 0 do
                local d
                absn, d = int_shortdiv(absn, base)
                s[#s+1] = ("0123456789ABCDEF"):sub(d+1, d+1)
            end
            s = concat(s:reverse())
            s = s:gsub("^0+", "")
            if s == "" then s = "0" end
        end
        if sign < 0 and s ~= "0" then s = "-" .. s end
        return s
    end

    local function int_iszero(a)
        return #a == 0
    end

    local function int_isone(a)
        return #a == 1 and a[1] == 1 and a.s == 1
    end

    local function int_istwo(a)
        return #a == 1 and a[1] == 2 and a.s == 1
    end

    local function int_cmp(a, b)
        if #a == 0 and #b == 0 then return 0 end -- 0 == -0
        if a.s ~= b.s then return a.s > b.s and 1 or -1 end
        if #a ~= #b then return #a > #b and a.s or -a.s end
        for i = #a, 1, -1 do
            if a[i] ~= b[i] then return a[i] > b[i] and a.s or -a.s end
        end
        return 0
    end

    local function int_abscmp(a, b)
        if #a ~= #b then return #a > #b and 1 or -1 end
        for i = #a, 1, -1 do
            if a[i] ~= b[i] then return a[i] > b[i] and 1 or -1 end
        end
        return 0
    end

    local function int_neg(a)
        local b = int_copy(a)
        b.s = -a.s
        return b
    end

    int_add = function(a, b)
        if a.s ~= b.s then return int_sub(a, int_neg(b)) end
        local c = int()
        c.s = a.s
        local carry = 0
        for i = 1, max(#a, #b) do
            c[i] = carry + (a[i] or 0) + (b[i] or 0)
            if c[i] >= RADIX then c[i], carry = c[i] - RADIX, 1
            else carry = 0 end
        end
        if carry > 0 then c[#c+1] = carry end
        return c
    end

    int_sub = function(a, b)
        if a.s ~= b.s then return int_add(a, int_neg(b)) end
        local A, B
        local cmp = int_abscmp(a, b)
        if cmp >= 0 then A = a; B = b; else A = b; B = a; end
        local c = int()
        local carry = 0
        for i = 1, #A do
            c[i] = A[i] - (B[i] or 0) - carry
            if c[i] < 0 then c[i], carry = c[i] + RADIX, 1
            else carry = 0 end
        end
        assert(carry == 0) -- should be true if |A| >= |B|
        c.s = (cmp >= 0) and a.s or -a.s
        int_trim(c)
        return c
    end

    int_mul = function(a, b)
        local c = int()
        for i = 1, #a + #b do c[i] = 0 end
        for i = 1, #a do
            local carry = 0
            for j = 1, #b do
                carry = c[i+j-1] + a[i]*b[j] + carry
                c[i+j-1] = carry % RADIX
                carry = carry // RADIX
            end
            if carry ~= 0 then c[i + #b] = carry end
        end
        int_trim(c)
        c.s = a.s * b.s
        return c
    end

    local function mul_scalar(a, k)
        local r, carry = int(), 0
        for i = 1, #a do
            local p = a[i]*k + carry
            r[i] = p % RADIX
            carry = p // RADIX
        end
        while carry > 0 do
            r[#r+1] = carry % RADIX
            carry = carry // RADIX
        end
        int_trim(r)
        return r
    end

    int_shortdiv = function(a, b)
        local q, r = int(), 0
        for i = #a, 1, -1 do
            local cur = r*RADIX + a[i]
            q[i] = cur // b
            r = cur % b
        end
        int_trim(q)
        return q, r
    end

    int_absdivmod = function(a, b)
        assert(not int_iszero(b), "Division by zero")
        if int_abscmp(a, b) < 0 then return int_zero, int_abs(a) end

        if #b == 1 then -- short division
            local q, r = int_shortdiv(a, b[1])
            return q, int(r)
        end

        -- Knuth algorithm (D)
        local n = #b        -- size of the divider
        local m = #a - n    -- size of the quotient
        -- D1: normalization
        local d = RADIX // (b[n] + 1)
        local u = mul_scalar(a, d)
        local v = mul_scalar(b, d)
        while #u < m+n+1 do u[#u+1] = 0 end -- u shall contain m+n+1 digits
        local vn = v[n]
        local vn1 = v[n-1]
        local q = int()
        for j = 1, m+1 do q[j] = 0 end
        -- D2-D7: main loop
        for j = m, 0, -1 do
            -- D3: estimate q̂
            local idx = j + n + 1
            local uj, uj1, uj2 = u[idx] or 0, u[idx-1] or 0, u[idx-2] or 0
            local num = uj*RADIX + uj1
            local qhat = num // vn
            local rhat = num % vn
            -- Correction 1: is q̂ >= RADIX or q̂*v[n-1] > RADIX*rhat + u[j+n-1]
            while true do
                if qhat >= RADIX then
                    qhat = RADIX-1
                    rhat = rhat + vn
                end
                if qhat*vn1 <= RADIX*rhat + uj2 then break end
                qhat = qhat - 1
                rhat = rhat + vn
                if rhat >= RADIX then break end
            end
            -- D4: substract q̂ * v shifted by u
            local borrow = 0
            for i = 1, n do
                local p = qhat * v[i]
                local sub_val = u[j+i] - p%RADIX - borrow
                borrow = p // RADIX
                if sub_val < 0 then
                    sub_val = sub_val + RADIX
                    borrow = borrow + 1
                end
                u[j+i] = sub_val
            end
            local top = u[j+n+1] - borrow
            u[j+n+1] = top
            -- D5: quotient digit
            q[j+1] = qhat
            -- D6: substract top
            if top < 0 then
                q[j+1] = qhat - 1
                local carry = 0
                for i = 1, n do
                    local s = u[j+i] + v[i] + carry
                    u[j+i], carry = s%RADIX, s//RADIX
                end
                u[j+n+1] = (u[j+n+1] + carry) % RADIX
            end
        end
        -- D8: remainder denormalization
        local r_norm = {}
        for i = 1, n do r_norm[i] = u[i] end
        local rem_carry = 0
        local r_final = int()
        for i = n, 1, -1 do
            local cur = rem_carry*RADIX + r_norm[i]
            r_final[i] = cur // d
            rem_carry = cur % d
        end

        int_trim(q)
        int_trim(r_final)
        return q, r_final
    end

    int_divmod = function(a, b)
        local q, r = int_absdivmod(a, b)
        q.s = a.s == b.s and 1 or -1
        return q, r
    end

    -- sqrt: integral Newton-Raphson iteration
    local function int_sqrt(a)
        assert(a.s >= 0, "Square root of a negative number")
        if int_iszero(a) then return int_zero end
        if int_isone(a) then return int_one end

        local p = #a
        local half = (p+1) // 2
        local b = int(0)
        b[half+1] = 1
        for i = 1, half do b[i] = 0 end
        while true do
            local q, _ = int_absdivmod(a, b)
            local s = int_add(b, q)
            local b_new, _ = int_shortdiv(s, 2)
            if int_abscmp(b_new, b) >= 0 then break end
            b = b_new
        end
        local b2 = int_mul(b, b)
        if int_abscmp(b2, a) > 0 then
            b = int_sub(b, int_one)
        else
            local b1 = int_add(b, int_one)
            if int_abscmp(int_mul(b1, b1), a) <= 0 then
                b = b1
            end
        end

        return b
    end

    local function int_pow(a, b)
        assert(b.s > 0)
        if int_iszero(b) then return int_one end
        if int_isone(b) then return a end
        if int_istwo(b) then return int_mul(a, a) end
        local c
        local q, r = int_shortdiv(b, 2)
        c = int_pow(a, q)
        c = int_mul(c, c)
        if r == 1 then c = int_mul(c, a) end
        return c
    end

    int_abs = function(a)
        local b = int_copy(a)
        b.s = 1
        int_trim(b)
        return b
    end

    local function int_iseven(a)
        return #a == 0 or a[1]%2 == 0
    end

    local function int_isodd(a)
        return #a > 0 and a[1]%2 == 1
    end

    local function int_gcd(a, b)
        a = int_abs(a)
        b = int_abs(b)
        if int_iszero(a) then return b end
        if int_iszero(b) then return a end
        local shift = 0
        while int_iseven(a) and int_iseven(b) do
            a = int_shortdiv(a, 2)
            b = int_shortdiv(b, 2)
            shift = shift + 1
        end
        while int_iseven(a) do a = int_shortdiv(a, 2) end
        while not int_iszero(b) do
            while int_iseven(b) do b = int_shortdiv(b, 2) end
            if int_abscmp(a, b) > 0 then a, b = b, a end
            b = int_sub(b, a)
        end
        for _ = 1, shift do a = int_add(a, a) end
        return a
    end

    local function int_lcm(a, b)
        a = int_abs(a)
        b = int_abs(b)
        return int_mul((int_absdivmod(a, int_gcd(a, b))), b)
    end

    local int_shift_left, int_shift_right

    int_shift_left = function(a, b)
        if int_iszero(b) then return a end
        if b.s > 0 then
            return int_mul(a, int_two^b)
        else
            return int_shift_right(a, int_neg(b))
        end
    end

    int_shift_right = function(a, b)
        if int_iszero(b) then return a end
        if b.s < 0 then
            return int_shift_left(a, int_neg(b))
        else
            return (int_divmod(a, int_two^b))
        end
    end

    mt.__add = function(a, b) return int_add(int(a), int(b)) end
    mt.__div = function(a, b) local q, _ = int_divmod(int(a), int(b)); return q end
    mt.__eq = function(a, b) return int_cmp(int(a), int(b)) == 0 end
    mt.__idiv = mt.__div
    mt.__le = function(a, b) return int_cmp(int(a), int(b)) <= 0 end
    mt.__lt = function(a, b) return int_cmp(int(a), int(b)) < 0 end
    mt.__mod = function(a, b) local _, r = int_divmod(int(a), int(b)); return r end
    mt.__mul = function(a, b) return int_mul(int(a), int(b)) end
    mt.__pow = function(a, b) return int_pow(int(a), int(b)) end
    mt.__shl = function(a, b) return int_shift_left(int(a), int(b)) end
    mt.__shr = function(a, b) return int_shift_right(int(a), int(b)) end
    mt.__sub = function(a, b) return int_sub(int(a), int(b)) end
    mt.__tostring = function(a, base) return int_tostring(a, base) end
    mt.__unm = function(a) return int_neg(a) end

    mt.__index.add = mt.__add
    mt.__index.bits = ni "bits"
    mt.__index.compare = function(a, b) return int_cmp(int(a), int(b)) end
    mt.__index.div = mt.__div
    mt.__index.egcd = ni "egcd"
    mt.__index.gcd = function(a, b) return int_gcd(int(a), int(b)) end
    mt.__index.invmod = ni "invmod"
    mt.__index.iseven = int_iseven
    mt.__index.isodd = int_isodd
    mt.__index.iszero = int_iszero
    mt.__index.isone = int_isone
    mt.__index.lcm = function(a, b) return int_lcm(int(a), int(b)) end
    mt.__index.mod = mt.__mod
    mt.__index.mul = mt.__mul
    mt.__index.neg = mt.__unm
    mt.__index.pow = mt.__pow
    mt.__index.powmod = ni "powmod"
    mt.__index.quotrem = function(a, b) return int_divmod(int(a), int(b)) end
    mt.__index.root = ni "root"
    mt.__index.shift = mt.__index.shl
    mt.__index.sign = function(a) return int_cmp(int(a), int_zero) end
    mt.__index.sqr = function(a) return int_mul(a, a) end
    mt.__index.sqrt = int_sqrt
    mt.__index.sub = mt.__sub
    mt.__index.abs = function(a) return int_abs(a) end
    mt.__index.tonumber = int_tonumber
    mt.__index.tostring = mt.__tostring
    mt.__index.totext = ni "totext"

    imath.abs = function(a) return int(a):abs() end
    imath.add = function(a, b) return int(a) + int(b) end
    imath.bits = function(a) return int(a):bits() end
    imath.compare = function(a, b) return int(a):compare(int(b)) end
    imath.div = function(a, b) return int(a) / int(b) end
    imath.egcd = function(a, b) return int(a):egcd(int(b)) end
    imath.gcd = function(a, b) return int(a):gcd(int(b)) end
    imath.invmod = function(a, b) return int(a):invmod(int(b)) end
    imath.iseven = function(a) return int(a):iseven() end
    imath.isodd = function(a) return int(a):isodd() end
    imath.iszero = function(a) return int(a):iszero() end
    imath.isone = function(a) return int(a):isone() end
    imath.lcm = function(a, b) return int(a):lcm(int(b)) end
    imath.mod = function(a, b) return int(a) % int(b) end
    imath.mul = function(a, b) return int(a) * int(b) end
    imath.neg = function(a) return -int(a) end
    imath.new = int
    imath.pow = function(a, b) return int(a) ^ b end
    imath.powmod = function(a, b) return int(a):powmod(int(b)) end
    imath.quotrem = function(a, b) return int(a):quotrem(int(b)) end
    imath.root = function(a) return int(a):root() end
    imath.shift = function(a, b) return int(a) << b end
    imath.sign = function(a) return int_cmp(int(a), int_zero) end
    imath.sqr = function(a) return int(a):sqr() end
    imath.sqrt = function(a) return int(a):sqrt() end
    imath.sub = function(a, b) return int(a) - int(b) end
    imath.text = ni "text"
    imath.tonumber = function(a) return int(a):tonumber() end
    imath.tostring = function(a) return int(a):tostring() end
    imath.totext = function(a) return int(a):totext() end

end

return imath
