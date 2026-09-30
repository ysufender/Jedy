local util = {}

---@param settings Project.Settings
function util.cc(settings, file)
    local base = string.match(file, "(.*)/.*")
    os.execute("mkdir -p build/"..base)
    return settings.cc..settings.cflags..file.." -o "..util.obj(file)
end

---@param settings Project.Settings
function util.ccxx(settings, file)
    local base = string.match(file, "(.*)/.*")
    os.execute("mkdir -p build/"..base)
    return settings.ccxx..settings.cxxflags..file.." -o "..util.obj(file)
end

function util.obj(file)
    return "build/"..file..".o"
end

return util
