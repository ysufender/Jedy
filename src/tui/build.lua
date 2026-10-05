local util = require "script.util"

local core = {}

local prefix = "src/tui/"

local sources = {
    prefix.."window.cpp",
    prefix.."input.cpp",
}

---@param settings Project.Settings
function core.build(settings, project)
    local core_step = settings.efile.Step
        .init("tui")
        :dependOnStep("setup")
        :dependOnFiles(sources)

    local link = settings.ld..settings.ldflags
    for _, source in ipairs(sources) do
        core_step:action(util.ccxx(settings, source))
        link = link..util.obj(source).." "
    end

    core_step:action(link.." -r -o "..util.obj(prefix:match("(.*)/")))
    project:step(core_step)
    return core_step.name
end

return core
