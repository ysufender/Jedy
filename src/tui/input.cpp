export module tui.input;

import core.input;

namespace tui::input {
    export class Input : public input::Input {
        public:
            static constexpr BufSize = 255;

        private:
            char buffer[BufSize] = {0};

        public:
            auto create(std::string_view const keyfile) -> Input {
                return {};
            }

            virtual auto poll() -> void override {
            }
    };
}
