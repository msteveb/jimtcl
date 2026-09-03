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

--[[------------------------------------------------------------------------@@@
## Additional functions (Lua)
@@@]]

-- Load fs.lua to add new methods to strings
--@LOAD=_

local has_fs, fs = pcall(require, "_fs")

local F = require "F"
local sys = require "sys"

local __PANDOC__, pandoc  = _ENV.PANDOC_VERSION ~= nil, _ENV.pandoc

if not has_fs then

    fs = {}

    local sh = require "sh"

    local __WINDOWS__ = sys.os == "windows"
    local __MACOS__   = sys.os == "macos"

    if __PANDOC__ then
        fs.sep = pandoc.path.separator
        fs.path_sep = pandoc.path.search_path_separator
    else
        fs.sep = package.config:match("^([^\n]-)\n")
        fs.path_sep = fs.sep == '\\' and ";" or ":"
    end

    local function safe_sh(...)
        local out, msg = sh.read(...)
        if not out then error(msg) end
        return out
    end

    function fs.getcwd()
        if __PANDOC__ then return pandoc.system.get_working_directory() end
        if __WINDOWS__ then return safe_sh "cd" : trim() end
        return safe_sh "pwd" : trim()
    end

    function fs.dir(path)
        if __PANDOC__ then return F(pandoc.system.list_directory(path)) end
        if __WINDOWS__ then return safe_sh("dir /b", path) : lines() : sort() end
        return safe_sh("ls", path) : lines() : sort()
    end

    fs.remove = os.remove

    fs.rename = os.rename

    function fs.copy(source_name, target_name)
        local from<close>, err_from = io.open(source_name, "rb")
        if not from then return from, err_from end
        local to<close>, err_to = io.open(target_name, "wb")
        if not to then return to, err_to end
        while true do
            local block = from:read(8*1024)
            if not block then break end
            local ok, err = to:write(block)
            if not ok then
                return ok, err
            end
        end
        return true
    end

    function fs.symlink(target, linkpath)
        if __WINDOWS__ then return nil, "symlink not implemented" end
        return sh.run("ln -s", target, linkpath)
    end

    function fs.mkdir(path)
        if __PANDOC__ then return pandoc.system.make_directory(path) end
        return sh.run("mkdir", path)
    end

    local S_IFMT  <const> = 0xF << 12
    local S_IFDIR <const> = 1 << 14
    local S_IFREG <const> = 1 << 15
    local S_IFLNK <const> = (1 << 13) | (1 << 15)

    local S_IRUSR <const> = 1 << 8
    local S_IWUSR <const> = 1 << 7
    local S_IXUSR <const> = 1 << 6
    local S_IRGRP <const> = 1 << 5
    local S_IWGRP <const> = 1 << 4
    local S_IXGRP <const> = 1 << 3
    local S_IROTH <const> = 1 << 2
    local S_IWOTH <const> = 1 << 1
    local S_IXOTH <const> = 1 << 0

    local S_IRALL <const> = 1 << 8 | 1 << 5 | 1 << 2
    local S_IWALL <const> = 1 << 7 | 1 << 4 | 1 << 1
    local S_IXALL <const> = 1 << 6 | 1 << 3 | 1 << 0

    fs.uR = S_IRUSR
    fs.uW = S_IWUSR
    fs.uX = S_IXUSR
    fs.aR = S_IRALL
    fs.aW = S_IWALL
    fs.aX = S_IXALL
    fs.gR = S_IRGRP
    fs.gW = S_IWGRP
    fs.gX = S_IXGRP
    fs.oR = S_IROTH
    fs.oW = S_IWOTH
    fs.oX = S_IXOTH

    local function stat(name, follow)
        local size, mtime, atime, ctime, mode
        if __MACOS__ then
            local st = sh.read("LC_ALL=C", "stat", follow, "-r", name, "2>/dev/null")
            if not st then return nil, "cannot stat "..name end
            local _, mode_str
            _, _, mode_str, _, _, _, _, size, atime, mtime, _, ctime, _, _, _ = st:words():unpack()
            mode = tonumber(mode_str, 8)
        else
            local st = sh.read("LC_ALL=C", "stat", follow, "-c '%s;%Y;%X;%W;%f'", name, "2>/dev/null")
            if not st then return nil, "cannot stat "..name end
            local mode_str
            size, mtime, atime, ctime, mode_str = st:trim():split ";":unpack()
            mode = tonumber(mode_str, 16)
        end
        return F{
            name = name,
            size = tonumber(size),
            mtime = tonumber(mtime),
            atime = tonumber(atime),
            ctime = tonumber(ctime),
            mode = mode,
            type = (mode & S_IFMT) == S_IFLNK and "link"
                or (mode & S_IFMT) == S_IFDIR and "directory"
                or (mode & S_IFMT) == S_IFREG and "file"
                or "unknown",
            uR = (mode & S_IRUSR) ~= 0,
            uW = (mode & S_IWUSR) ~= 0,
            uX = (mode & S_IXUSR) ~= 0,
            gR = (mode & S_IRGRP) ~= 0,
            gW = (mode & S_IWGRP) ~= 0,
            gX = (mode & S_IXGRP) ~= 0,
            oR = (mode & S_IROTH) ~= 0,
            oW = (mode & S_IWOTH) ~= 0,
            oX = (mode & S_IXOTH) ~= 0,
            aR = (mode & (S_IRUSR|S_IRGRP|S_IROTH)) ~= 0,
            aW = (mode & (S_IWUSR|S_IWGRP|S_IWOTH)) ~= 0,
            aX = (mode & (S_IXUSR|S_IXGRP|S_IXOTH)) ~= 0,
        }
    end

    function fs.stat(name)
        return stat(name, "-L")
    end

    function fs.lstat(name)
        return stat(name, {})
    end

    function fs.inode(name)
        local dev, ino
        if __MACOS__ then
            local st = sh.read("LC_ALL=C", "stat", "-L", "-r", name, "2>/dev/null")
            if not st then return nil, "cannot stat "..name end
            dev, ino = st:words():unpack()
        else
            local st = sh.read("LC_ALL=C", "stat", "-L", "-c '%d;%i'", name, "2>/dev/null")
            if not st then return nil, "cannot stat "..name end
            dev, ino = st:trim():split ";":unpack()
        end
        return F{
            ino = tonumber(ino),
            dev = tonumber(dev),
        }
    end

    local pattern_cache = {}

    function fs.fnmatch(pattern, name)
        local lua_pattern = pattern_cache[pattern]
        if not lua_pattern then
            lua_pattern = pattern
                : gsub("%.", "%%.")
                : gsub("%*", ".*")
            lua_pattern = "^"..lua_pattern.."$"
            pattern_cache[pattern] = lua_pattern
        end
        return name:match(lua_pattern) and true or false
    end

    function fs.chmod(name, ...)
        local mode = {...}
        if type(mode[1]) == "string" then
            return sh.run("chmod", "--reference="..mode[1], name, "2>/dev/null")
        else
            return sh.run("chmod", ("%o"):format(F(mode):fold(F.op.bor, 0)), name)
        end
    end

    function fs.touch(name, opt)
        if opt == nil then
            return sh.run("touch", name, "2>/dev/null")
        elseif type(opt) == "number" then
            return sh.run("touch", "-d", '"'..os.date("%c", opt)..'"', name, "2>/dev/null")
        elseif type(opt) == "string" then
            return sh.run("touch", "--reference="..opt, name, "2>/dev/null")
        else
            error "bad argument #2 to touch (none, nil, number or string expected)"
        end
    end

    function fs.basename(path)
        if __PANDOC__ then return pandoc.path.filename(path) end
        return (path:gsub(".*[/\\]", ""))
    end

    function fs.dirname(path)
        if __PANDOC__ then return pandoc.path.directory(path) end
        local dir, n = path:gsub("[/\\][^/\\]*$", "")
        return n > 0 and dir or "."
    end

    function fs.splitext(path)
        if __PANDOC__ then
            if fs.basename(path):match "^%." then return path, "" end
            return pandoc.path.split_extension(path)
        end
        local name, ext = path:match("^(.*)(%.[^/\\]-)$")
        if name and ext and #name > 0 and not name:has_suffix(fs.sep) then return name, ext end
        return path, ""
    end

    function fs.ext(path)
        local _, ext = fs.splitext(path)
        return ext
    end

    function fs.chext(path, new_ext)
        return fs.splitext(path) .. new_ext
    end

    function fs.realpath(path)
        if __PANDOC__ then return pandoc.path.normalize(path) end
        return safe_sh("realpath", path) : trim()
    end

    function fs.readlink(path)
        return safe_sh("readlink", path) : trim()
    end

    function fs.absname(path)
        if path:match "^[/\\]" or path:match "^.:" then return path end
        return fs.getcwd()..fs.sep..path
    end

    function fs.mkdirs(path)
        if __PANDOC__ then return pandoc.system.make_directory(path, true) end
        if __WINDOWS__ then return sh.run("mkdir", path) end
        return sh.run("mkdir", "-p", path)
    end

    function fs.ls(dir, dotted) ---@diagnostic disable-line: unused-local (hidden files not supported in the Lua implementation)
        dir = dir or "."
        local base = dir:basename()
        local path = dir:dirname()
        local recursive = base:match"%*%*"
        local pattern = base:match"%*" and base:gsub("%*+", "*")

        local useless_path_prefix = "^%."..fs.sep
        local function clean_path(fullpath)
            return fullpath:gsub(useless_path_prefix, "")
        end

        if __WINDOWS__ then

            local files
            if recursive then
                files = sh("dir /b /s", path/pattern)
            elseif pattern then
                files = sh("dir /b", path/pattern)
            else
                files = sh("dir /b", dir)
            end
            return (files or "") : lines()
                : map(clean_path)
                : sort()
        end

        local files
        if recursive then
            files = (sh("find", path, ("-name %q"):format(pattern), "2>/dev/null") or "") : lines()
                : filter(F.partial(F.op.ne, path))
        elseif pattern then
            files = (sh("ls -d", path/pattern, "2>/dev/null") or "") : lines()
        else
            files = (sh("ls", dir, "2>/dev/null") or "") : lines()
                : map(F.partial(fs.join, dir))
        end
        return files
            : map(clean_path)
            : sort()
    end

    function fs.is_file(name)
        local st = fs.stat(name)
        return st ~= nil and st.type == "file"
    end

    function fs.is_dir(name)
        local st = fs.stat(name)
        return st ~= nil and st.type == "directory"
    end

    function fs.is_link(name)
        local st = fs.lstat(name)
        return st ~= nil and st.type == "link"
    end

    fs.rm = fs.remove
    fs.mv = fs.rename

    fs.tmpfile = os.tmpname

    function fs.tmpdir()
        local tmp = os.tmpname()
        fs.rm(tmp)
        fs.mkdir(tmp)
        return tmp
    end

end

--[[@@@
```lua
fs.join(...)
```
> return a path name made of several path components
> (separated by `fs.sep`).
> If a component is absolute, the previous components are removed.
@@@]]

function fs.join(...)
    local function clean_path(p)
        -- remove duplicate and trailing separators
        return (p : gsub("[/\\]+", fs.sep) : gsub("[/\\]+$", ""))
    end
    local function add_path(ps, p)
        if p:match("^"..fs.sep) then return F{p} end
        ps[#ps+1] = p
        return ps
    end
    return clean_path(F.flatten{...}:fold(add_path, F{}):str(fs.sep))
end

--[[@@@
```lua
fs.splitpath(path)
```
> return a list of path components.
@@@]]

function fs.splitpath(path)
    if path == "" then return F{} end
    local components = path:split "[/\\]+"
    if components[1] == "" then components[1] = fs.sep end
    return components
end

--[[@@@
```lua
fs.findpath(name)
```
> returns the full path of `name` if `name` is found in `$PATH` or `nil`.
@@@]]

function fs.findpath(name)
    local function exists_in(path) return fs.is_file(fs.join(path, name)) end
    local path = os.getenv("PATH")
        :split(fs.path_sep)
        :find(exists_in)
    if path then return fs.join(path, name) end
    return nil, name..": not found in $PATH"
end

--[[@@@
```lua
fs.rmdir(path)
```
> deletes the directory `path` and its content recursively.
@@@]]

function fs.rmdir(path)
    if __PANDOC__ then
        pandoc.system.remove_directory(path, true)
        return true
    end
    fs.walk(path, {reverse=true}):foreach(fs.rm)
    return fs.rm(path)
end

--[[@@@
```lua
fs.walk([path], [{reverse=true|false, links=true|false, cross=true|false}])
```
> returns a list listing directory and
> file names in `path` and its subdirectories (the default path is the current
> directory).
>
> Options:
>
> - `stat`: returns the list of stat results instead of just filenames
> - `reverse`: the list is built in a reverse order
>   (suitable for recursive directory removal)
> - `cross`: walk across several devices
> - `func`: function applied to the current file or directory.
>   `func` takes two parameters (path of the file or directory and the stat object returned by `fs.stat`)
>   and returns a boolean (to continue or not walking recursively through the subdirectories)
>   and the value to add to the list.
@@@]]

function fs.walk(path, options)
    options = options or {}
    local return_stat = options.stat
    local reverse = options.reverse
    local cross_device = options.cross
    local func = options.func
              or return_stat and function(_, stat) return true, stat end
              or function(name, _) return true, name end
    local dirs = {path or "."}
    local acc_files = {}
    local acc_dirs = {}
    local seen = {}
    local dev0 = nil
    local function already_seen(name)
        local inode = fs.inode(name)
        if not inode then return true end
        dev0 = dev0 or inode.dev
        if dev0 ~= inode.dev and not cross_device then
            return true
        end
        if not seen[inode.dev] then
            seen[inode.dev] = {[inode]=true}
            return false
        end
        if not seen[inode.dev][inode.ino] then
            seen[inode.dev][inode.ino] = true
            return false
        end
        return true
    end
    while #dirs > 0 do
        local dir = table.remove(dirs)
        if not already_seen(dir) then
            local names = fs.dir(dir)
            if names then
                table.sort(names)
                for i = 1, #names do
                    local name = dir..fs.sep..names[i]
                    local stat = fs.stat(name)
                    if stat then
                        if stat.type == "directory" then
                            local continue, obj = func(name, stat)
                            if continue then
                                dirs[#dirs+1] = name
                            end
                            if obj then
                                if reverse then table.insert(acc_dirs, 1, obj)
                                else acc_dirs[#acc_dirs+1] = obj
                                end
                            end
                        else
                            local _, obj = func(name, stat)
                            if obj then
                                acc_files[#acc_files+1] = obj
                            end
                        end
                    end
                end
            end
        end
    end
    return F.concat(reverse and {acc_files, acc_dirs} or {acc_dirs, acc_files})
end

--[[@@@
```lua
fs.with_tmpfile(f)
```
> calls `f(tmp)` where `tmp` is the name of a temporary file.
@@@]]

function fs.with_tmpfile(f)
    if __PANDOC__ then
        return pandoc.system.with_temporary_directory("luax", function(tmpdir)
            return f(tmpdir/"tmpfile")
        end)
    end
    local tmp = fs.tmpfile()
    local ret = table.pack(f(tmp))
    fs.rm(tmp)
    return table.unpack(ret)
end

--[[@@@
```lua
fs.with_tmpdir(f)
```
> calls `f(tmp)` where `tmp` is the name of a temporary directory.
@@@]]

function fs.with_tmpdir(f)
    if __PANDOC__ then
        return pandoc.system.with_temporary_directory("luax", f)
    end
    local tmp = fs.tmpdir()
    local ret = table.pack(f(tmp))
    fs.rmdir(tmp)
    return table.unpack(ret)
end

--[[@@@
```lua
fs.with_dir(path, f)
```
> changes the current working directory to `path` and calls `f()`.
@@@]]

function fs.with_dir(path, f)
    if __PANDOC__ then
        return pandoc.system.with_working_directory(path, f)
    end
    if fs.chdir then
        local old = fs.getcwd()
        fs.chdir(path)
        local ret = table.pack(f(path))
        fs.chdir(old)
        return table.unpack(ret)
    end
    error "fs.with_dir not implemented"
end

--[[@@@
```lua
fs.expand(path, [vars])
```
> returns the expanded path
> where `"~"` at the beginning of the path is replaced by the home directory of the current user
> and `$XXX` or `${XXX}` is replaced by the environment variable `XXX`.
> Variable values can also be taken from the optional `vars` table.
@@@]]

local function expanduser(path)
    local home = os.getenv(sys.os == "windows" and "USERPROFILE" or "HOME")
    if path == "~" then return home end
    local local_path = path:match "^~([/\\].*)"
    if local_path then return home..local_path end
    return path
end

local function expandvars(path, vars)
    vars = vars or {}
    local function expand(var) return os.getenv(var) or vars[var] or var end
    path = path : gsub("${([%w_]+)}", expand) : gsub("$([%w_]+)", expand)
    return path
end

function fs.expand(path, vars)
    return expandvars(expanduser(path), vars)
end

--[[@@@
```lua
fs.read(filename)
```
> returns the content of the text file `filename`.
@@@]]

function fs.read(name)
    local f<close>, oerr = io.open(name, "r")
    if not f then return f, oerr end
    return f:read("a")
end

--[[@@@
```lua
fs.write(filename, ...)
```
> write `...` to the text file `filename`.
@@@]]

function fs.write(name, ...)
    local content = F{...}:flatten():str()
    local f<close>, oerr = io.open(name, "w")
    if not f then return f, oerr end
    return f:write(content)
end

--[[@@@
```lua
fs.read_bin(filename)
```
> returns the content of the binary file `filename`.
@@@]]

function fs.read_bin(name)
    local f<close>, oerr = io.open(name, "rb")
    if not f then return f, oerr end
    return f:read("a")
end

--[[@@@
```lua
fs.write_bin(filename, ...)
```
> write `...` to the binary file `filename`.
@@@]]

function fs.write_bin(name, ...)
    local content = F{...}:flatten():str()
    local f<close>, oerr = io.open(name, "wb")
    if not f then return f, oerr end
    return f:write(content)
end

--[[------------------------------------------------------------------------@@@
## String methods

Some functions of the `fs` package are added to the string module:

@@@]]

--[[@@@
```lua
path:stat()             == fs.stat(path)
path:inode()            == fs.inode(path)
path:basename()         == fs.basename(path)
path:dirname()          == fs.dirname(path)
path:splitext()         == fs.splitext(path)
path:ext()              == fs.ext(path)
path:chext()            == fs.chext(path)
path:realpath()         == fs.realpath(path)
path:readlink()         == fs.readlink(path)
path:absname()          == fs.absname(path)
path1 / path2           == fs.join(path1, path2)
path:is_file()          == fs.is_file(path)
path:is_dir()           == fs.is_dir(path)
path:findpath()         == fs.findpath(path)
```
@@@]]

string.stat             = fs.stat
string.inode            = fs.inode
string.basename         = fs.basename
string.dirname          = fs.dirname
string.splitext         = fs.splitext
string.ext              = fs.ext
string.chext            = fs.chext
string.splitpath        = fs.splitpath
string.realpath         = fs.realpath
string.readlink         = fs.readlink
string.absname          = fs.absname
string.is_file          = fs.is_file
string.is_dir           = fs.is_dir
string.findpath         = fs.findpath

getmetatable("").__div  = fs.join

return fs
