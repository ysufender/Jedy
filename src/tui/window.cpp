module;
#include <unistd.h>

export module tui.window;

import std;

import ftxui.screen;

import core.window;
import core.buffer.manager;
import core.rawmode;
import core.input;

namespace tui::window {
    struct WindowContext {
        core::buffer::BufferManager& manager;
        ftxui::Screen& screen;
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

                int const width = context.screen.dimx();
                int const height = context.screen.dimy();

                int row = 1;
                int col = 1;
                for (char const ch : found.value()->view()) {
                    context.screen.CellAt(col, row).inverted = false;

                    if (row >= height) {
                        break;
                    }
                    else if (ch == '\n') {
                        ++row;
                        col = 0;
                        continue;
                    }
                    else if (ch == '\r') {
                        continue;
                    }
                    else if (ch == '\t') {
                        col += 4 - (col % 4);
                        continue;
                    }
                    else if (col < width) {
                        context.screen.CellAt(col, row).character = std::string(1, ch);
                    }
                    ++col;
                }

                auto const cx = static_cast<int>(this->pos.col);
                auto const cy = static_cast<int>(this->pos.line);
                if (cx < width && cy < height) {
                    context.screen.CellAt(cx, cy).inverted = true;
                }

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

        public:
            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : core::window::Window(manager, std::move(input)),
                  rawmode(),
                  closing(false),
                  screen(ftxui::Screen::Create(ftxui::Dimension::Full())) { }

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
                char c;
                if (::read(STDIN_FILENO, &c, 1) != 1) {
                    return std::nullopt;
                }

                if (c == 0x03) {
                    this->close();
                    return std::nullopt;
                }
                else if (!std::iscntrl(c)) {
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
            }

            auto draw() -> std::optional<std::string_view> override {
                if (this->panes.empty()) {
                    return "No panes to draw.";
                }

                WindowContext context {
                    .manager = this->manager,
                    .screen = this->screen,
                };

                if (auto const err = this->panes[this->active]->draw(&context)) {
                    return err;
                }

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
