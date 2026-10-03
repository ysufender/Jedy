export module core.window;

import std;

import core.buffer.manager;

namespace core::window {
    export struct Position {
        std::size_t line;
        std::size_t column;
    };

    export class Window {
        public:
            virtual auto addPane(std::string_view const) -> std::optional<std::string_view> = 0;
            virtual auto draw() -> std::optional<std::string_view> = 0;
            virtual auto switchPane(int const) -> std::optional<std::string_view> = 0;
    };

    export class Pane {
        protected:
            std::string_view activeBuffer = "";
            Position pos = {1, 1};
            Window& window;

            Pane(Window& window, std::string_view const buf)
                : window(window),
                  activeBuffer(buf) { }

        public:
            virtual auto draw(core::buffer::BufferManager&) -> std::optional<std::string_view> = 0;
            virtual auto setpos(Position const) -> std::optional<std::string_view> = 0;
            inline auto getpos() -> Position { return this->pos; }
    };
}
