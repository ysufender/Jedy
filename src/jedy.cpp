import std;

import core.cli;

auto main(int const argc, char const* const* const args) -> int {
    cli::Args cli = cli::init<2>();

    cli.option("gui", false, "Open editor in GUI mode.")
        .param("file", cli::Value::make<cli::Value::Type::string>("./buffer.txt"), "file to open");

    cli.help();

    return 0;
}
