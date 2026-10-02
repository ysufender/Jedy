export module core.window;

import std;

namespace window {
    export struct Position {
        std::size_t line;
        std::size_t column;
    };

    export class Pane {
        protected:
            std::string_view activeBuffer = "";
            Position pos = {1, 1};

        public:
            virtual auto draw() -> std::optional<std::string_view> = 0;
            virtual auto setpos(Position const) -> std::optional<std::string_view> = 0;

            constexpr auto getpos() -> Position { return this->pos; }

            template<class PaneType>
                requires(
                    std::is_base_of_v<Pane, PaneType>
                    && std::is_same_v<decltype(PaneType::create("")), std::optional<PaneType>>
                )
            static auto create(std::string_view const from) -> std::optional<PaneType> {
                return PaneType::create(from);
            }
    };

    export class IWindow {
        public:
            virtual auto addPane(std::string_view const) -> std::optional<std::string_view> = 0;
            virtual auto draw() -> std::optional<std::string_view> = 0;
            virtual auto switchPane(int const) -> std::optional<std::string_view> = 0;
    };

    export template<class WindowType, int MaxPane>
    concept Window = requires(WindowType w) {
        requires std::is_base_of_v<Pane, typename WindowType::Pane>;
        { w.panes } -> std::same_as<std::array<typename WindowType::Pane, MaxPane>&>;
        { w.activePane } -> std::same_as<int&>;

        { w.addPane("buffer_name") } -> std::same_as<std::optional<std::string_view>>;
        { w.draw() } -> std::same_as<std::optional<std::string_view>>;
        { w.switchPane(5) } -> std::same_as<std::optional<std::string_view>>;
    };

    export template<class WindowType, int MaxPane>
        requires Window<WindowType, MaxPane>
    auto create() -> std::optional<WindowType> {
        return WindowType::create();
    }
}
