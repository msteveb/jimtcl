-- Fennel launcher script
-- arg[0] is this script path (set by our lua wrapper)
local script_path = arg[0] or ""
local fennel_dir = script_path:match("(.*/)")
if fennel_dir then
    -- go up from build-support to repo root, then to src/fennel
    fennel_dir = fennel_dir .. "../src/fennel"
else
    fennel_dir = "/workspaces/jim/src/fennel"
end

package.path = fennel_dir .. "/?.lua;" .. package.path
local fennel = require("fennel")

local i = 1
if #arg == 0 or (arg[1] == nil) then
    fennel.repl()
    os.exit(0)
end

while i <= #arg do
    local a = arg[i]
    if a == "--version" or a == "-v" then
        print("Fennel " .. fennel.version .. " on " .. _VERSION)
        os.exit(0)
    elseif a == "--repl" then
        fennel.repl()
        os.exit(0)
    elseif a == "-e" or a == "--eval" then
        i = i + 1
        local result, err = fennel.eval(arg[i])
        if err then io.stderr:write(err .. "\n"); os.exit(1) end
        if result ~= nil then print(result) end
    elseif a == "--" then
        i = i + 1
        break
    else
        local ok, err = pcall(fennel.dofile, a)
        if not ok then io.stderr:write(tostring(err) .. "\n"); os.exit(1) end
        os.exit(0)
    end
    i = i + 1
end
