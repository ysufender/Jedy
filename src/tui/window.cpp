export module tui.window;

import std;

import window;

namespace tui {
    export class Pane : public window::Pane {
        public:
            virtual auto draw() -> std::optional<std::string_view>> = 0;
            virtual auto setpos(Position const) -> std::optional<std::string_view>> = 0;

            inline auto getpos() -> Position { return this->pos; }
    };

    export template<class T, int MaxPane>
    concept Window = requires(T w) {
        { w.panes } -> std::same_as<std::array<Pane, MaxPane>>;
        { w.activePane } -> std::same_as<int>;
        { w.create() } -> std::same_as<std::optional<std::string_view>>;
        { w.addPane("buffer_name") } -> std::same_as<std::optional<std::string_view>>;
        { w.draw() } -> std::same_as<std::optional<std::string_view>>;
        { w.switch(5) } -> std::same_as<std::optional<std::string_view>>;
    };

    export template<class WindowType, int MaxPane>
        requires Window<WindowType, MaxPane>
    auto create() -> std::optional<WindowType> {
        return WindowType::create();
    }
}
