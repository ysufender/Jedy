export module core.input;

import std;

namespace core::input {
    export struct Input {
        virtual auto poll() -> void = 0;
        virtual auto await(unsigned int) -> void = 0;
        virtual auto input(std::optional<std::string_view>) -> std::string = 0;
        virtual auto check(unsigned int) -> bool = 0;
    };
}
