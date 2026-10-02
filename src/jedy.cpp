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
    auto const file = cli.get("file").value_or(nullptr);

    std::ifstream input;
    input.open(file->payload.string.data(), std::ios::in);
    if (!input.is_open()) {
        std::println("Failed to open file at path '{}'", file->payload.string);
        return 1;
    }

    std::size_t const end = input
                            .seekg(0, std::ios::end)
                            .tellg();

    auto maybeBuffman = core::buffer::BufferManager::create();
    if (!maybeBuffman) {
        std::println("Error while setting up buffer manager.");
        return 1;
    }

    auto buffman = maybeBuffman.value();

    auto const result = buffman.buffer(file->payload.string, end);
    if (result) {
        std::println("Error: {}", result.value());
        return 1;
    }

    input.seekg(0, std::ios::beg);
    input.read(buffman.get(file->payload.string).value()->buffer, end);

    auto window = core::window::create<tui::window::Window<1>, 1>(buffman);
    if (!window) {
        std::println("Failed to create window.");
        return 1;
    }

    window.value().addPane(file->payload.string);

    while (true) {
        window.value().draw();
    }

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
