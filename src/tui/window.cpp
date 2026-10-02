export module tui.window;

import std;

import ftxui;

import core.window;
import core.buffer.manager;

namespace tui::window {
    export template<int MaxPane>
    class Window {
        private:
            core::buffer::BufferManager& manager;
            ftxui::Screen screen;

            Window(ftxui::Screen&& screen, core::buffer::BufferManager& manager)
                : manager(manager),
                  screen(screen),
                  panes(),
                  activePane(0) { }

        public:
            class Pane : public core::window::Pane {
                private:
                    std::string_view buffer = "";

                    Pane(std::string_view const buf) : buffer(buf) { }

                public:
                    inline auto bufferName() const -> std::string_view {
                        return this->buffer;
                    }

                    virtual auto draw(core::buffer::BufferManager& manager) -> std::optional<std::string_view> override {
                        auto const buf = manager.get(this->buffer);

                        if (buf) {
                            return std::make_optional(buf.value()->buffer);
                        }

                        return std::nullopt;
                    }

                    virtual auto setpos(core::window::Position const) -> std::optional<std::string_view> override {
                        return std::nullopt;
                    }

                    static auto create(core::buffer::BufferManager& manager, std::string_view const from) -> std::optional<Pane> {
                        auto const buf = manager.get(from);

                        if (!buf) {
                            return std::nullopt;
                        }

                        return Pane{from};
                    }
            };

            std::array<std::optional<Pane>, MaxPane> panes;
            int activePane;

        public:
            static auto create(core::buffer::BufferManager& manager) -> std::optional<Window> {
                return std::make_optional(Window{
                    ftxui::Screen::Create(ftxui::Dimension::Full()),
                    manager,
                });
            }

            auto addPane(std::string_view const name) -> std::optional<std::string_view> {
                std::optional<Pane>* free = nullptr;

                for (auto& pane : this->panes) {
                    if (!pane) {
                        free = &pane;
                    }
                }

                if (!free) {
                    return std::make_optional("Reached max pane count.");
                }

                auto const pane = Pane::create(this->manager, name);
                if (!pane) {
                    return std::make_optional("Failed to create pane.");
                }

                *free = pane;
                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> {
                auto const body = ftxui::vbox({
                    ftxui::vbox({
                        ftxui::text(this->panes[this->activePane].value().draw(this->manager).value_or("<error>")) | ftxui::flex,
                        ftxui::separator(),
                        ftxui::text(this->panes[this->activePane].value().bufferName()) | ftxui::dim,
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
