local util = require "script.util"

local src = {}

local prefix = "src/"

local subdirs = {
    "core",
    "tui",
}

local sources = {
    prefix.."jedy.cpp"
}

---@param settings Project.Settings
function src.build(settings, project)
    local src_step = settings.efile.Step
        .init("src")

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

    for _, subdir in ipairs(subdirs) do
        link = link..util.obj(prefix..subdir).." "
        src_step:dependOnStep(
            require("src."..subdir..".build").build(settings, project))
    end

    for vendor, vendor_settings in pairs(settings.vendor) do
        link = link..util.vendor(vendor, vendor_settings.modules).." "
    end

    src_step:action(link.." -o build/jedy")
    src_step:dependOnSteps(compiles)
    project:step(src_step)
    return src_step.name
end

return src
