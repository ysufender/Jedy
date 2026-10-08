module;
#include <unistd.h>

export module tui.input;

import std;

import core.input;

namespace tui::input {
    export class Input : public core::input::Input {
        public:
            Input() : core::input::Input() { }

            auto poll() -> std::optional<core::input::InputType> override {
                using core::input::InputType;

                if (!this->pending.empty()) {
                    auto const top = this->pending.front();
                    this->pending.pop();
                    return top;
                }

                unsigned char c;
                if (::read(STDIN_FILENO, &c, 1) != 1) {
                    return std::nullopt;
                }

                if (c == 0x7f || c == 0x08) {
                    return InputType::Backspace;
                }
                if (c != 0x1b) {
                    return static_cast<InputType>(c);
                }

                unsigned char a, b;
                if (::read(STDIN_FILENO, &a, 1) != 1) {
                    return InputType::Escape;
                }

                if (a != '[' && a != 'O') {
                    this->pending.emplace(static_cast<InputType>(a));
                    return InputType::Escape;
                }

                if (::read(STDIN_FILENO, &b, 1) != 1) {
                    return InputType::Escape;
                }

                switch (b) {
                    case '1': case '3': case '4': case '7': case '8': {
                        unsigned char t;
                        if (::read(STDIN_FILENO, &t, 1) == 1 && t == '~') {
                            if (b == '3') {
                                return InputType::Delete;
                            }
                        }
                        return std::nullopt;
                    }
                    default: return std::nullopt;
                }
            }
    };
}
