local util = require "script.util"

local src = {}

local prefix = "src/"

local subdirs = {
    "core"
}

local sources = {
    prefix.."jedy.cpp"
}

---@param settings Project.Settings
function src.build(settings, project)
    local src_step = settings.efile.Step
        .init("src")
        :dependOnStep("setup")
        :dependOnFiles(sources)

    local link = settings.ld..settings.ldflags
    for _, subdir in ipairs(subdirs) do
        link = link..util.obj(prefix..subdir).." "
        src_step:dependOnStep(
            require("src."..subdir..".build").build(settings, project))
    end

    for _, source in ipairs(sources) do
        src_step:action(util.ccxx(settings, source))
        link = link..util.obj(source).." "
    end


    src_step:action(link.." -o build/jedy")
    project:step(src_step)
    return src_step.name
end

return src
