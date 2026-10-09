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

    local compiles = {}

    local link = settings.ld..settings.ldflags
    for _, source in ipairs(sources) do
        table.insert(compiles, source..".o")
        local action = util.ccxx(settings, source)

        project:step(settings.efile.Step
            .init(source..".o")
            :dependOnFile(source)
            :dependOnStep("setup")
            :dependOnFile(prefix.."build.lua")
            :action(action))

        link = link..util.obj(source).." "
    end

    core_step:action(link.." -r -o "..util.obj(prefix:match("(.*)/")))
    core_step:dependOnSteps(compiles)
    project:step(core_step)
    return core_step.name
end

return core
