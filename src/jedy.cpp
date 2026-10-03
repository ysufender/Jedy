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
    (void)cli;
    std::println("TUI mode is not (yet) implemented.");
    return 1;
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
