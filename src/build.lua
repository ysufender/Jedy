local util = require "script.util"

local src = {}

local prefix = "src/"

local subdirs = {}

local sources = {
    prefix.."jedy.cpp"
}

---@param settings Project.Settings
function src.build(settings)
    local substeps = {}

    for _, subdir in ipairs(subdirs) do
        table.insert(substeps, require("src."..subdir..".build").build(settings))
    end

    local compile = "echo 'Start'"
    local link = settings.ld..settings.ldflags
    for _, source in ipairs(sources) do
        compile = compile.." && "..util.ccxx(settings, source)
        link = link..util.obj(source).." "
    end


    return settings.efile.Step
        .init("src")
        :dependOnStep("setup")
        :dependOnFiles(sources)
        :dependOnSteps(substeps)
        :action(compile)
        :action(link)
end

return src
