local Efile = require "efile"

---@class Project.Settings
local settings = {
    name     = "jedy",
    version  = "0.0.1",
    cc       = "gcc ",
    ccxx     = "g++ ",
    ld       = "g++ ",
    cflags   = " ",
    cxxflags = "-std=c++26 -fmodules -c -Wall -Wextra -Werror -fconcepts-diagnostics-depth=5 ",
    ldflags  = " ",

    efile    = Efile,

    vendor   = {
        ["ftxui"] = {
            "ftxui-modules",
            "ftxui-screen",
            "ftxui-dom",
            "ftxui-component"
        }
    }
}

local ccxx_version = ""

local file = io.popen(settings.ccxx.."-dumpversion", "r")
if not file then
    print("Failed to get "..settings.ccxx.."version")
    os.exit(1)
end

ccxx_version = file:read("l")
file:close()

local function install_ftxui()
    local ftxui = {}

    file = io.open("vendor/ftxui/ftxui.zip", "r")
    if file then
        ftxui = {
            Efile.Step
                .init("ftxui")
                :action("echo 'ftxui is already installed'"),
        }

        file:close()
    else
        ftxui = {
            Efile.Step
                .init("ftxui-install")
                :dependOnFiles("build.lua")
                :action("mkdir -p vendor/ftxui")
                :action("curl -fsSL https://github.com/ArthurSonzogni/FTXUI/archive/refs/tags/v7.0.3.zip -o vendor/ftxui/ftxui.zip")
                :action("echo 'Downloading FTXUI...'")
                :action("unzip -o vendor/ftxui/ftxui.zip -d vendor/ftxui")
                :action("mv -f -n vendor/ftxui/FTXUI-7.0.3/* vendor/ftxui")
                :action("rm -rf vendor/ftxui/FTXUI-7.0.3"),

            Efile.Step
                .init("ftxui")
                :dependOnStep("ftxui-install")
                :action("cmake -S vendor/ftxui/ -B vendor/ftxui/build -G Ninja -DFTXUI_BUILD_MODULES=ON -DCMAKE_CXX_STANDARD=26")
                :action("cmake --build vendor/ftxui/build")
                :action("mkdir -p gcm.cache/CMakeFiles/ftxui-modules.dir")
                :action("cp -rf vendor/ftxui/build/CMakeFiles/ftxui-modules.dir/*.gcm gcm.cache/CMakeFiles/ftxui-modules.dir/"),
        }
    end

    return ftxui
end

local project = Efile.Project
    .init(settings.name)

project
    :step(Efile.Step
        .init("prerequisites")
        :dependOnFile("/usr/include/c++/"..ccxx_version.."/bits/std.cc")
        :action(settings.ccxx.."-std=c++26 -fmodules -fmodule-only -c /usr/include/c++/"..ccxx_version.."/bits/std.cc"))

    :multiStep(install_ftxui())

    :step(Efile.Step
        .init("setup")
        :dependOnSteps({
            "ftxui"
        })
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
        .init("clean")
        :action("rm -rf build")
        :action((function()
            if arg[2] == "all" then
                return "rm -rf vendor"
            end
        end)()))

require("src.build").build(settings, project)

print(project:build(arg[1]) or "Success")
