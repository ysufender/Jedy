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

    :step(Efile.Step
        .init("all")
        :dependOnSteps({
            "src",
        }))

    :step(Efile.Step
        .init("setup")
        :dependOnFiles({
            "/usr/include/c++/16.2.0/bits/std.cc",
            "/usr/include/c++/16.2.0/bits/std.compat.cc"
        })
        :action("echo 'int main(int argc, char** args) { return 0; }' > build/dummy.cpp ")
        :action("g++ -std=c++26 -fmodules --compile-std-module -c build/dummy.cpp -o build/dummy.o"))

    :step(require("src.build").build(settings))

    :step(Efile.Step
        .init("clean")
        :action("rm -rf build"))

print(project:build(arg[1]) or "Success")
