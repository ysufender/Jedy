export module tui.window;

import std;

import ftxui.screen;

import core.window;
import core.buffer.manager;
import core.rawmode;

namespace tui::window {
    struct WindowContext {
        core::buffer::BufferManager& manager;
        ftxui::Screen& screen;
    };

    export class Pane : public core::window::Pane {
        public:
            Pane(std::string_view const buffer)
                : core::window::Pane(buffer) { }
 
            auto draw(void* const ctx) -> std::optional<std::string_view> final {
                auto& context = *static_cast<WindowContext*>(ctx);

                auto const found = context.manager.get(this->activeBuffer);
                if (!found) {
                    return "Failed to get current buffer.";
                }

                int const width = context.screen.dimx();
                int const height = context.screen.dimy();

                int row = 0;
                int col = 0;
                for (char const ch : found.value()->view()) {
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

            auto setpos(core::Position const p) -> std::optional<std::string_view> final {
                this->pos = p;
                return std::nullopt;
            }
    };

    export class Window : public core::window::Window {
        private:
            core::RawMode rawmode;
            bool closing;

        public:
            Window(core::buffer::BufferManager& manager)
                : core::window::Window(manager),
                  rawmode(),
                  closing(false) { }

            auto addPane(std::string_view const buffer) -> std::optional<std::string_view> final {
                try {
                    this->panes.emplace_back(std::make_unique<Pane>(buffer));
                } catch (std::exception const&) {
                    return "Failed to add pane.";
                }

                this->active = this->panes.size() - 1;
                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> final {
                if (this->panes.empty()) {
                    return "No panes to draw.";
                }

                auto screen = ftxui::Screen::Create(ftxui::Dimension::Full());

                WindowContext context {
                    .manager = this->manager,
                    .screen = screen,
                };

                if (auto const err = this->panes[this->active]->draw(&context)) {
                    return err;
                }

                std::cout << "\x1b[H";
                std::cout << screen.ToString();
                std::cout << std::flush;

                return std::nullopt;
            }

            auto switchPane(int const delta) -> std::optional<std::string_view> final {
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
