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
# Minimal tar file support

```lua
local tar = require "tar"
```

The `tar` module can read and write tar archives.

**Limitations**:

- Only files, directories and symbolic links are supported.
- File names longer than 100 bytes are rejected.                                                                                                   ▆
- Files larger than 8 GiB are rejected.
- Only classic USTAR format is supported (no PAX, no long names).
@@@]]

-- https://fr.wikipedia.org/wiki/Tar_%28informatique%29

local tar = {}

local F = require "F"
local fs = require "fs"
local sys = require "sys"

local __WINDOWS__ = sys.os == "windows"

local format = string.format
local pack   = string.pack
local unpack = string.unpack
local rep    = string.rep
local bytes  = string.bytes
local sum    = F.sum

local function pad(size)
    return (512 - size%512) % 512
end

local file_type = F{
    file = "0",
    link = "2",
    directory = "5",
}

local rev_file_type = file_type:mapk2a(function(k, v) return {v, k} end):from_list()
rev_file_type["\0"] = rev_file_type["0"]

local default_mode = {
    file = tonumber("644", 8),
    link = tonumber("777", 8),
    directory = tonumber("755", 8),
}

local Discarded = {}

local function path_components(path)
    local function is_sep(d) return d==fs.sep or d=="." or d==" " end
    return fs.splitpath(path):drop_while(is_sep):drop_while_end(is_sep)
end

local function clean_path(path)
    return fs.join(path_components(path))
end

local function header(st, xform)
    local name = xform(clean_path(st.name))
    if name == nil then return Discarded end
    if #name > 100 then return nil, name..": filename too long" end
    if st.type=="file" and st.size >= 8*1024^3 then return nil, st.name..": file too big" end
    local ftype = file_type[st.type]
    if not ftype then return nil, st.name..": wrong file type" end
    if st.type=="link" and not st.link then return nil, st.name..": missing link name" end
    if st.link and #st.link > 100 then return nil, st.link..": filename too long" end
    local header1 = pack("c100c8c8c8c12c12",
        name,
        format("%07o", st.mode or default_mode[st.type] or "0"),
        "",
        "",
        format("%011o", st.size or 0),
        format("%011o", st.mtime)
    )
    local header2 = pack("c1c100c6c2c32c32c8c8c155c12",
        ftype,
        st.link or "",
        "", "", "", "", "", "", "", ""
    )
    local checksum = format("%07o", sum(bytes(header1)) + sum(bytes(header2)) + 32*8)
    return header1..pack("c8", checksum)..header2
end

local function end_of_archive()
    return pack("c1024", "")
end

local function parse(archive, i)
    local name, mode, _, _, size, mtime, checksum, ftype, link = unpack("c100c8c8c8c12c12c8c1c100", archive, i)
    if not checksum then return nil, "Corrupted archive" end
    local function cut(s) return s:match "^[^\0]*" end
    if sum(bytes(archive:sub(i, i+148-1))) + sum(bytes(archive:sub(i+156, i+512-1))) + 32*8 ~= tonumber(cut(checksum), 8) then
        return nil, "Wrong checksum"
    end
    ftype = rev_file_type[ftype]
    if not ftype then return nil, cut(name)..": wrong file type" end
    return {
        name = cut(name),
        mode = tonumber(cut(mode), 8),
        size = tonumber(cut(size), 8),
        mtime = tonumber(cut(mtime), 8),
        type = ftype,
        link = ftype=="link" and cut(link) or nil,
    }
end

--[[@@@
```lua
tar.tar(files, [xform])
```
> returns a string that can be saved as a tar file.
> `files` is a list of file names or `stat` like structures.
> `stat` structures shall contain these fields:
>
> - `name`: file name
> - `mtime`: last modification time
> - `content`: file content (the default value is the actual content of the file `name`).
>
> **Note**: these structures can also be produced by `fs.stat`.
>
> `xform` is an optional function used to transform filenames in the archive.
@@@]]

function tar.tar(files, xform)
    xform = xform or F.id
    local chunks = F{}

    local already_done = {}
    local function done(name)
        if already_done[name] then return true end
        already_done[name] = true
        return false
    end

    local function add_dir(path, st0)
        if done(path) then return true end
        if path:dirname() == path then return true end
        local ok, err = add_dir(path:dirname(), st0)
        if not ok then return nil, err end
        local st = F.merge{st0, { name=path, mode=tonumber("755", 8), size=0, type="directory" }}
        local hd
        hd, err = header(st, F.id)
        if not hd then return nil, err end
        chunks[#chunks+1] = hd
        return true
    end

    local function add_file(st)
        local xformed_name = xform(st.name)
        if xformed_name == nil then return true end
        if done(xformed_name) then return true end
        local ok, err = add_dir(xformed_name:dirname(), st)
        if not ok then return nil, err end
        local hd
        hd, err = header(st, xform)
        if hd == Discarded then return true end
        if not hd then return nil, err end
        chunks[#chunks+1] = hd
        chunks[#chunks+1] = st.content
        chunks[#chunks+1] = rep("\0", pad(#st.content))
        return true
    end

    local function add_link(st)
        local xformed_name = xform(st.name)
        if xformed_name == nil then return true end
        if done(xformed_name) then return true end
        local ok, err = add_dir(xformed_name:dirname(), st)
        if not ok then return nil, err end
        local hd
        hd, err = header(st, xform)
        if hd == Discarded then return true end
        if not hd then return nil, err end
        chunks[#chunks+1] = hd
        return true
    end

    local function add_real_dir(path)
        if done(path) then return true end
        if path:dirname() == path then return true end
        local ok, err = add_real_dir(path:dirname())
        if not ok then return nil, err end
        local st
        st, err = fs.stat(path)
        if not st then return nil, err end
        local hd
        hd, err = header(st, xform)
        if hd == Discarded then return true end
        if not hd then return nil, err end
        chunks[#chunks+1] = hd
        return true
    end

    local function add_real_file(st)
        local xformed_name = xform(st.name)
        if xformed_name == nil then return true end
        if done(xformed_name) then return true end
        local ok, err = add_real_dir(st.name:dirname())
        if not ok then return nil, err end
        local hd
        hd, err = header(st, xform)
        if hd == Discarded then return true end
        if not hd then return nil, err end
        local content
        content, err = fs.read_bin(st.name)
        if not content then return nil, err end
        chunks[#chunks+1] = hd
        chunks[#chunks+1] = content
        chunks[#chunks+1] = rep("\0", pad(#content))
        return true
    end

    local function add_real_link(st)
        local xformed_name = xform(st.name)
        if xformed_name == nil then return true end
        if done(xformed_name) then return true end
        local linkst, sterr = fs.stat(st.name)
        if not linkst then return nil, sterr end
        local ok, err = add_real_dir(st.name:dirname())
        if not ok then return nil, err end
        local hd
        st.link = linkst.name
        hd, err = header(st, xform)
        if hd == Discarded then return true end
        if not hd then return nil, err end
        chunks[#chunks+1] = hd
        return true
    end

    for _, file in ipairs(files) do

        if type(file) == "string" then
            local st, err = fs.stat(file)
            if not st then return nil, err end
            if st.type == "file" then
                add_real_file(st)
            elseif st.type == "link" then
                add_real_link(st)
            elseif st.type == "directory" then
                add_real_dir(st.name)
                for _, name in ipairs(fs.ls(st.name/"**")) do
                    local childst, childerr = fs.stat(name)
                    if not childst then return nil, childerr end
                    if childst.type == "directory" then
                        add_real_dir(childst.name)
                    elseif childst.type == "file" then
                        add_real_file(childst)
                    end
                end
            end

        elseif type(file) == "table" then
            local st0 = nil
            local err
            local st = {
                name = file.name,
                type = "file",
            }
            if file.content then
                st.content = file.content
                st.size = #file.content
            elseif file.link then
                st.type = "link"
                st.link = file.link
            else
                if __WINDOWS__ then
                    st0, err = fs.stat(file.name)
                else
                    st0, err = fs.lstat(file.name)
                end
                if not st0 then return nil, err end
                if st0.type == "link" then
                    local linkst, linkerr = fs.stat(file.name)
                    if not linkst then return nil, linkerr end
                    st.type = "link"
                    st.link = linkst.name
                else
                    local content
                    content, err = fs.read_bin(file.name)
                    if not content then return nil, err end
                    st.size = st0.size
                    st.content = content
                end
            end
            if file.mtime then
                st.mtime = file.mtime
            else
                st.mtime = st0 and st0.mtime or os.time()
            end
            local ok
            if st.type == "link" then
                ok, err = add_link(st)
            else
                ok, err = add_file(st)
            end
            if not ok then return nil, err end

        end

    end

    chunks[#chunks+1] = end_of_archive()
    return chunks:str()

end

--[[@@@
```lua
tar.untar(archive, [xform])
```
> returns a list of files (`stat` like structures with a `content` field).
>
> `xform` is an optional function used to transform filenames in the archive.
@@@]]

function tar.untar(archive, xform)
    xform = xform or F.id
    if #archive % 512 ~= 0 then return nil, "Corrupted archive" end
    local eof = end_of_archive()
    local files = F{}
    local i = 1
    while i <= #archive do
        if archive:byte(i, i) == 0 then
            if archive:sub(i, i+#eof-1):is_prefix_of(eof) then break end
            return nil, "Corrupted archive"
        end
        local st, err = parse(archive, i)
        if not st then return nil, err end
        if st.type == "file" then
            st.content = archive:sub(i+512, i+512+st.size-1)
            i = i + 512 + st.size + pad(st.size)
        elseif st.type == "link" or st.type == "directory" then
            i = i + 512
        else
            return nil, st.type..": file type not supported"
        end
        st.name = xform(st.name)
        if st.name ~= nil then
            files[#files+1] = st
        end
    end
    return files
end

--[[@@@
```lua
tar.chain(xforms)
```
> returns a filename transformation function that applies all functions from `funcs`.
@@@]]

function tar.chain(funcs)
    return function(x)
        for _, f in ipairs(funcs) do
            x = f(clean_path(x))
            if x == nil then return nil end
        end
        return clean_path(x)
    end
end

--[[@@@
```lua
tar.strip(x)
```
> returns a transformation function that removes part of the beginning of a filename.
> If `x` is a number, the function removes `x` path components in the filename.
> If `x` is a string, the function removes `x` at the beginning of the filename.
@@@]]

function tar.strip(x)
    if type(x) == "number" then
        return function(path)
            local dirs = path_components(path:dirname())
            if x > #dirs then return nil end
            return clean_path(fs.join(dirs:drop(x))/path:basename())
        end
    else
        local prefix = clean_path(x)
        return function(path)
            path = clean_path(path)
            if path:has_prefix(prefix) then
                return clean_path(path:sub(#prefix+1))
            else
                return path
            end
        end
    end
end

--[[@@@
```lua
tar.add(p)
```
> returns a transformation function that adds `p` at the beginning of a filename.
@@@]]

function tar.add(p)
    local prefix = path_components(p)
    return function(path)
        local components = path_components(path)
        return clean_path(fs.join(prefix..components))
    end
end

--[[@@@
```lua
tar.xform(x, y)
```
> returns a transformation function that chains `tar.strip(x)` and `tar.add(y)`.
@@@]]

function tar.xform(x, y)
    return tar.chain { tar.strip(x), tar.add(y) }
end

return tar
