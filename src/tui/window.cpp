module;
#include <unistd.h>

export module tui.window;

import std;

import ftxui;

import core.window;
import core.buffer.manager;
import core.rawmode;
import core.input;

namespace tui::window {
    enum class Mode {
        Navigation,
        Input,
        Selection,
    };

    constexpr std::string_view ModeStr[] = {
        "Navigation",
        "Input",
        "Selection",
    };

    struct WindowContext {
        core::buffer::BufferManager& manager;
        ftxui::Screen& screen;
        ftxui::Element element;
    };

    export class Pane : public core::window::Pane {
        public:
            Pane(std::string_view const buffer)
                : core::window::Pane(buffer) { }
 
            auto draw(void* const ctx) -> std::optional<std::string_view> override {
                auto& context = *static_cast<WindowContext*>(ctx);

                auto const found = context.manager.get(this->activeBuffer);
                if (!found) {
                    return "Failed to get current buffer.";
                }

                context.element = ftxui::text(found.value()->view());

                return std::nullopt;
            }

            auto setpos(core::Position const p) -> std::optional<std::string_view> override {
                this->pos = p;
                return std::nullopt;
            }
    };

    export class Window : public core::window::Window {
        private:
            core::RawMode rawmode;
            bool closing;
            ftxui::Screen screen;
            Mode mode;

        public:
            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : core::window::Window(manager, std::move(input)),
                  rawmode(),
                  closing(false),
                  screen(ftxui::Screen::Create(ftxui::Dimension::Full())),
                  mode(Mode::Navigation) { }

            auto addPane(std::string_view const buffer) -> std::optional<std::string_view> override {
                try {
                    this->panes.emplace_back(std::make_unique<Pane>(buffer));
                } catch (std::exception const&) {
                    return "Failed to add pane.";
                }

                this->active = this->panes.size() - 1;
                return std::nullopt;
            }

            auto process() -> std::optional<std::string_view> override {
                auto const input = this->input->poll();

                if (!input) {
                    return std::nullopt;
                }

                auto c = input.value();

                if (c == core::input::Terminate) {
                    this->close();
                }
                else switch (this->mode) {
                    case Mode::Navigation: goto navigation;
                    case Mode::Input: goto input;
                    case Mode::Selection: goto selection;
                }


input:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                }
                else if (std::isalnum(c)) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const res = found.value()->append(pane->getpos().off, c);
                    if (res) {
                        return res;
                    }

                    pane->setpos({pane->getpos().col + 1, pane->getpos().line, pane->getpos().off + 1});
                }
                return std::nullopt;

navigation:
                if (c == core::input::I || c == core::input::i) {
                    this->mode = Mode::Input;
                }
                return std::nullopt;

selection:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                }
                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> override {
                if (this->panes.empty()) {
                    return "No panes to draw.";
                }

                this->screen = ftxui::Screen::Create(ftxui::Dimension::Full());

                WindowContext context {
                    .manager = this->manager,
                    .screen = this->screen,
                    .element = ftxui::text("No Buffer"),
                };

                auto const res = this->panes.at(this->active)->draw(&context);

                auto const body = ftxui::flex(
                    ftxui::vbox(
                        ftxui::flex(context.element),
                        ftxui::bgcolor(
                            res ? ftxui::Color::Red : ftxui::Color::Black,
                            ftxui::text(ModeStr[static_cast<int>(this->mode)])
                        ),
                        ftxui::color(
                            ftxui::Color::Black,
                            ftxui::bgcolor(
                                ftxui::Color::Green,
                                ftxui::text(this->panes.at(this->active)->getname())
                            )
                        )
                    )
                );

                ftxui::Render(this->screen, body);
                std::cout << "\x1b[H";
                std::cout << screen.ToString();
                std::cout << std::flush;

                return std::nullopt;
            }

            auto switchPane(int const delta) -> std::optional<std::string_view> override {
                if (this->panes.empty()) {
                    return "No panes to switch.";
                }

                auto const n = static_cast<std::ptrdiff_t>(this->panes.size());
                auto const next = (static_cast<std::ptrdiff_t>(this->active) + delta % n + n) % n;
                this->active = static_cast<std::size_t>(next);
                return std::nullopt;
            }

            auto close() -> void override {
                this->closing = true;
            }

            auto shouldClose() -> bool override {
                return closing;
            }
    };
}
