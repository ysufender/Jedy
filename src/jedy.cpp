import std;

import core.cli;
import buffer.manager;

auto main(int const argc, char const* const* const args) -> int {
    cli::Args cli = cli::init<2>(argc, args);

    auto const status = cli.option("gui", false, "Open editor in GUI mode.")
                        .param("file", cli::Value::make<cli::Value::Type::string>("./default.txt"), "file to open")
                        .parse();

    if (status) {
        std::println("Error while parsing command line: {}", status.value());
        return 1;
    }

    if (cli.get_unwrap<cli::Value::Type::boolean>("gui")) {
        std::println("GUI mode is not (yet) implemented.");
        return 1;
    }

    auto const file = cli.get("file").value_or(nullptr);

    std::ifstream input;
    input.open(file->payload.string.data(), std::ios::in);
    if (!input.is_open()) {
        std::println("Failed to open file at path '{}'", file->payload.string);
        return 1;
    }

    std::size_t const end = input
                            .seekg(0, std::ios::beg)
                            .tellg();

    auto maybeBuffman = buffer::BufferManager::create();
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
    
    return 0;
}
