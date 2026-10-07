module;
#include <unistd.h>

export module tui.window;

import std;

import ftxui;

import core.window;
import core.buffer.manager;
import core.buffer;
import core.rawmode;
import core.input;

namespace tui::window {
    enum class Mode {
        Navigation,
        Input,
        Selection,
        Command,
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
            std::stringstream cmdBuf;

        public:
            static constexpr std::size_t DefaultCommandBufferSize = 255;

            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : core::window::Window(manager, std::move(input)),
                  rawmode(),
                  closing(false),
                  screen(ftxui::Screen::Create(ftxui::Dimension::Full())),
                  mode(Mode::Navigation),
                  cmdBuf("") {
            }

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
                    case Mode::Command: goto command;
                }

input:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    this->cmdBuf.str(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Backspace) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const res = found.value()->modify(pane->getpos().off, ' ');
                    if (res) {
                        return res;
                    }

                    auto const pos = pane->getpos();
                    if (pos.col > 1) {
                        return pane->setpos({pos.line, pos.col - 1, pos.off - 1});
                    }
                    return std::nullopt;
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

                    auto const pos = pane->getpos();
                    return pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                }
                return std::nullopt;

navigation:
                if (c == core::input::I || c == core::input::i) {
                    this->mode = Mode::Input;
                    this->cmdBuf.str(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::At) {
                    this->mode = Mode::Command;
                    this->cmdBuf.str("");
                }
                return std::nullopt;

selection:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    this->cmdBuf.str(ModeStr[static_cast<int>(this->mode)]);
                }
                return std::nullopt;

command:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    this->cmdBuf.str(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (std::isalnum(c)) {
                    this->cmdBuf << static_cast<char>(c);
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
                            res ? ftxui::Color::Red : ftxui::Color(ftxui::Color::Palette1::Default),
                            ftxui::text(this->cmdBuf.view())
                        ),
                        ftxui::color(
                            ftxui::Color::Black,
                            ftxui::bgcolor(
                                ftxui::Color::Green,
                                ftxui::hbox(
                                    ftxui::text(this->panes.at(this->active)->getname()),
                                    ftxui::text(":"),
                                    ftxui::text(std::to_string(this->panes.at(this->active)->getpos().line)),
                                    ftxui::text(":"),
                                    ftxui::text(std::to_string(this->panes.at(this->active)->getpos().col))
                                )
                            )
                        )
                    )
                );

                ftxui::Render(this->screen, body);
                auto const pos = this->panes.at(this->active)->getpos();
                this->screen.CellAt(pos.col - 1, pos.line - 1).inverted = true;
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
