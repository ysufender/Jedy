export module tui.input;

import core.input;

namespace tui::input {
    export class Input : public core::input::Input {
        public:
            auto poll() -> void override {
            }
    };
}
