export module tui.window;

import std;

import ftxui;

import core.window;

namespace tui::window {
    export template<int MaxPane>
    class Window {
        private:
            ftxui::Screen screen;
            std::array<Pane, MaxPane> panes;
            int activePane;
            int count;

            Window(ftxui::Screen&& screen)
                : screen(screen),
                  panes(),
                  activePane(0),
                  count(0) { }

        public:
            class Pane : public core::window::Pane {
                private:
                    std::string_view buffer;

                public:
                    virtual auto draw() -> std::optional<std::string_view> override {
                        return std::nullopt;
                    }

                    virtual auto setpos(core::window::Position const) -> std::optional<std::string_view> override {
                        return std::nullopt;
                    }

                    static auto create(std::string_view const from) -> std::optional<Pane> {
                        auto const buf = /* TODO: Continue */
                    }
            };

            static auto create() -> std::optional<Window> {
                return std::make_optional(Window{
                    ftxui::Screen::Create(ftxui::Dimension::Full()),
                });
            }

            auto addPane(std::string_view const name) -> std::optional<std::string_view> {
                if (this->count >= MaxPane) {
                    return std::make_optional("Reached max pane count.");
                }

                this->panes[this->count++] = 

                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> {
                auto const body = ftxui::vbox({
                    ftxui::text("Jedy") | ftxui::bold | ftxui::center,
                    ftxui::separator(),
                    ftxui::vbox({
                        ftxui::text("") | ftxui::flex,
                        ftxui::separator(),
                        ftxui::text("Footer Information") | ftxui::dim,
                      }) | ftxui::border | ftxui::flex,
                });

                ftxui::Render(this->screen, body);
                this->screen.Print();
                return std::nullopt;
            }

            auto switchPane(int const pnum) -> std::optional<std::string_view> {
                return std::nullopt;
            }
    };
}
