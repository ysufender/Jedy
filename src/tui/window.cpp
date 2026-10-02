export module tui.window;

import std;

import window;

namespace tui::window {
    export template<int MaxPane>
    class Window {
        private:
            Window() : panes(), activePane(0) { }

        public:
            class Pane : public window::Pane {
                public:
                    virtual auto draw() -> std::optional<std::string_view> override {
                        return std::nullopt;
                    }

                    virtual auto setpos(window::Position const) -> std::optional<std::string_view> override {
                        return std::nullopt;
                    }
            };

            std::array<P)ane, MaxPane> panes;
            int activePane;

            static auto create() -> std::optional<Window> {
                return { };
            }

            auto addPane(std::string_view const name) -> std::optional<std::string_view> {
                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> {
                return std::nullopt;
            }

            auto switchPane(int const pnum) -> std::optional<std::string_view> {
                return std::nullopt;
            }
    };
}
