export module core.window;

import std;

import core.buffer.manager;
import core.input;

namespace core {
    export struct Position {
        std::size_t line;
        std::size_t col;
        std::size_t off;

        auto operator==(Position other) -> bool {
            return this->line == other.line
                   && this->col == other.col;
        }

        auto same(std::size_t line, std::size_t col) -> bool {
            return this->line == line
                   && this->col == col;
        }
    };
}

namespace core::window {
    export class Window;

    export class Pane {
        protected:
            std::string activeBuffer;
            Position pos;

        public:
            Pane(std::string_view const buf)
                : activeBuffer(buf),
                  pos({1, 1, 0}) { }

            virtual auto draw(void* const) -> std::optional<std::string_view> { return std::nullopt; }
            virtual auto setpos(core::Position const) -> std::optional<std::string_view> { return std::nullopt; }

            inline auto getpos() -> core::Position { return this->pos; }
            inline auto getname() -> std::string const& { return this->activeBuffer; }
    };

    export class Window {
        protected:
            std::vector<std::unique_ptr<Pane>> panes;
            int active;
            core::buffer::BufferManager& manager;
            std::unique_ptr<core::input::Input> input;

        public:
            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : panes(),
                  active(0),
                  manager(manager),
                  input(std::move(input)) { }

            virtual auto addPane(std::string_view const) -> std::optional<std::string_view> = 0;
            virtual auto removePane(unsigned int const) -> std::optional<std::string_view> = 0;
            virtual auto draw() -> std::optional<std::string_view> = 0;
            virtual auto process() -> std::optional<std::string_view> = 0;
            virtual auto switchPane(int const) -> std::optional<std::string_view> = 0;
            virtual auto close() -> void = 0;
            virtual auto shouldClose() -> bool = 0;
    };
}
