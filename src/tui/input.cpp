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
                char terminput;
                if (::read(STDIN_FILENO, &terminput, 1) != 1) {
                    goto done;
                }

                core::input::InputType inputType;
                if (std::isalnum(terminput)) {
                    inputType = static_cast<core::input::InputType>(terminput);
                }
                else {
                    inputType = static_cast<core::input::InputType>(terminput);
                }
                this->pending.emplace(inputType);

done:
                if (this->pending.empty()) {
                    return std::nullopt;
                }

                auto const top = this->pending.front();
                this->pending.pop();
                return top;
            }
    };
}
