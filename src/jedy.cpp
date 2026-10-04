#include <unistd.h>

import std;

import core.cli;
import core.buffer.manager;
import core.window;
import tui.window;

constexpr int ArgCount = 2;

auto gui_mode(core::cli::Args<ArgCount> const& cli) -> int {
    (void)cli;
    std::println("GUI mode is not (yet) implemented.");
    return 1;
}

auto tui_mode(core::cli::Args<ArgCount> const& cli) -> int {
    core::buffer::BufferManager buffman {};

    auto const file = cli.get_unwrap<core::cli::Value::Type::string>("file");

    std::size_t size { 0 };
    std::string text { "Empty file" };

    {
        std::ifstream source { file.data() };
        if (source.is_open()) {
            source.seekg(0, std::ios::end);
            size = source.tellg();
            source.seekg(0, std::ios::beg);
            text = std::string{std::istreambuf_iterator<char>{source}, {}};
        }
        else {
            std::ofstream source { file.data() };
            if (!source.is_open()) {
                std::println("Error: Failed to open file {}", file);
                return 1;
            }
        }
    }

    auto res = buffman.buffer(file, size);
    if (res) {
        std::println("Error: Failed to create buffer. {}", res.value());
        return 1;
    }
    buffman.get(file).value()->assign(text);

    auto const input = std::make_unique<tui::input::Input>();

    tui::window::Window window { buffman, input };
    res = window.addPane(file);
    if (res) {
        std::println("Error: Failed to create window. {}", res.value());
        return 1;
    }

    do {
        window.input();
        window.draw();
    } while (!window.shouldClose());

    return 0;
}

auto main(int const argc, char const* const* const args) -> int {
    core::cli::Args cli = core::cli::init<ArgCount>(argc, args);

    auto const status = cli.option("gui", false, "Open editor in GUI mode.")
                        .param("file", core::cli::Value::make<core::cli::Value::Type::string>("./default.txt"), "file to open")
                        .parse();

    if (status) {
        std::println("Error while parsing command line: {}", status.value());
        return 1;
    }

    return (cli.get_unwrap<core::cli::Value::Type::boolean>("gui"))
        ? gui_mode(cli)
        : tui_mode(cli);
}
