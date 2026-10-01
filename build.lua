local Efile = require "efile"

---@class Project.Settings
local settings = {
    name     = "jedy",
    version  = "0.0.1",
    cc       = "gcc ",
    ccxx     = "g++ ",
    ld       = "g++ ",
    cflags   = " ",
    cxxflags = "-std=c++26 -fmodules -c ",
    ldflags  = " ",

    efile    = Efile,
}

local project = Efile.Project
    .init(settings.name)

project
    :step(Efile.Step
        .init("prerequisites")
        :dependOnFiles({
            "build.lua",
            "src/build.lua",
            "src/core/build.lua"
        }))

    :step(Efile.Step
        .init("all")
        :dependOnSteps({
            "src",
        }))

    :step(Efile.Step
        .init("setup")
        :dependOnStep("prerequisites")
        :action(settings.ccxx.."-std=c++26 -fmodules -fsearch-include-path -fmodule-only -c bits/std.cc"))

    :step(Efile.Step
        .init("clean")
        :action("rm -rf build"))

require("src.build").build(settings, project)

print(project:build(arg[1]) or "Success")
