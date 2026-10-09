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

auto nthOccurrence(std::string_view const text, char const target, std::size_t const n) -> std::size_t {
    std::size_t idx = 0;
    std::size_t from = 0;
    for (std::size_t i = 0; i < n; ++i) {
        idx = text.find(target, from);
        if (idx == std::string_view::npos) {
            return std::string_view::npos;
        }
        from = idx + 1;
    }
    return idx;
}

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

                auto const half = static_cast<std::size_t>(std::max((context.screen.dimy() - 2) / 2, 0));
                auto const topLine = this->pos.line > half ? this->pos.line - half : 1;
                auto const text = found.value()->view();

                std::size_t start = 0;
                if (topLine > 1) {
                    auto const nl = nthOccurrence(text, '\n', topLine - 1);
                    if (nl == std::string_view::npos) {
                        return "Cursor line is out of range.";
                    }
                    start = nl + 1;
                }

                context.element = ftxui::paragraph(text.substr(start));

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
            std::string_view cmdBuf;

        public:
            static constexpr std::size_t DefaultCommandBufferSize = 255;

            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : core::window::Window(manager, std::move(input)),
                  rawmode(),
                  closing(false),
                  screen(ftxui::Screen::Create(ftxui::Dimension::Full())),
                  mode(Mode::Navigation),
                  cmdBuf("<command_buffer>") {
                manager.buffer("<command_buffer>", 255);
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

                auto commandBuffer = this->manager.get("<command_buffer>").value();

                if (c == core::input::ETX) {
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
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Backspace) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const pos = pane->getpos();
                    if (pos.col <= 1) {
                        if (pos.line > 1 && pos.off >= 1) {
                            auto const before = found.value()->view();
                            auto const crlf = pos.off >= 2
                                && before[pos.off - 2] == '\r'
                                && before[pos.off - 1] == '\n';

                            auto res = found.value()->remove(pos.off - 1);
                            if (res) { return res; }
                            if (crlf) {
                                res = found.value()->remove(pos.off - 2);
                                if (res) { return res; }
                            }

                            auto const end = pos.off - (crlf ? 2 : 1);
                            auto const after = found.value()->view();
                            auto const nl = after.substr(0, end).rfind('\n');
                            auto const lineStart = (nl == std::string_view::npos) ? 0 : nl + 1;
                            auto const col = end - lineStart + 1;

                            return pane->setpos({pos.line - 1, col, end});
                        }
                        return std::nullopt;
                    }

                    auto const res = found.value()->remove(pos.off - 1);
                    if (res) {
                        return res;
                    }

                    return pane->setpos({pos.line, pos.col - 1, pos.off - 1});
                }
                else if (c == core::input::Delete) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const pos = pane->getpos();
                    auto const cursor = found.value()->get(pos.off);
                    if (!cursor || cursor.value() == '\0' || cursor.value() == core::input::ETX) {
                        return std::nullopt;
                    }

                    if (cursor.value() == '\r') {
                        auto const res = found.value()->remove(pos.off);
                        if (res) { return res; }

                        auto const next = found.value()->get(pos.off);
                        if (next && next.value() == '\n') {
                            return found.value()->remove(pos.off);
                        }
                        return std::nullopt;
                    }

                    return found.value()->remove(pos.off);
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

                    auto const pos = pane->getpos();
                    return pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                }
                else if (c == core::input::CR || c == core::input::LF) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const pos = pane->getpos();
                    auto res = found.value()->append(pos.off, '\r');
                    if (res) {
                        return res;
                    }
                    res = found.value()->append(pos.off + 1, '\n');
                    if (res) {
                        return res;
                    }

                    return pane->setpos({pos.line + 1, 1, pos.off + 2});
                }
                return std::nullopt;

navigation:
                if (c == core::input::i) {
                    this->mode = Mode::Input;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::a) {
                    this->mode = Mode::Input;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);

                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    auto const maybeBuf = this->manager.get(pane->getname());

                    if (!maybeBuf) {
                        return std::nullopt;
                    }

                    auto const next = maybeBuf.value()->get(pos.off);
                    if (next
                        && next.value() != '\r'
                        && next.value() != '\n'
                        && next.value() != '\0'
                        && next.value() != core::input::ETX) {
                        pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                    }
                }
                else if (c == core::input::Colon) {
                    this->mode = Mode::Command;
                    commandBuffer->assign("Command >>> ");
                }
                else if (c == core::input::h) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    if (pos.col > 1) {
                        pane->setpos({pos.line, pos.col - 1, pos.off - 1});
                    }
                }
                else if (c == core::input::l) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    auto const maybeBuf = this->manager.get(pane->getname());

                    if (!maybeBuf) {
                        return std::nullopt;
                    }

                    auto const next = maybeBuf.value()->get(pos.off);
                    if (next
                        && next.value() != '\r'
                        && next.value() != '\n'
                        && next.value() != '\0'
                        && next.value() != core::input::ETX) {
                        pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                    }
                }
                else if (c == core::input::k) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    if (pos.line <= 1) {
                        return std::nullopt;
                    }

                    auto const maybeBuf = this->manager.get(pane->getname());
                    if (!maybeBuf) {
                        return std::nullopt;
                    }

                    auto const text = maybeBuf.value()->view();
                    auto const lineStart = pos.off - (pos.col - 1);
                    if (lineStart < 1) {
                        return std::nullopt;
                    }

                    auto const nl = text.substr(0, lineStart - 1).rfind('\n');
                    auto const prevStart = (nl == std::string_view::npos) ? 0 : nl + 1;
                    auto const crlf = lineStart >= 2 && text[lineStart - 2] == '\r';
                    auto const prevEnd = lineStart - 1 - (crlf ? 1 : 0);
                    auto const len = prevEnd - prevStart;
                    auto const maxIdx = len > 0 ? len - 1 : 0;
                    auto const idx = std::min<std::size_t>(pos.col - 1, maxIdx);

                    return pane->setpos({pos.line - 1, idx + 1, prevStart + idx});
                }
                else if (c == core::input::j) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    auto const maybeBuf = this->manager.get(pane->getname());
                    if (!maybeBuf) {
                        return std::nullopt;
                    }

                    auto const text = maybeBuf.value()->view();
                    auto const nl = text.find('\n', pos.off);
                    if (nl == std::string_view::npos) {
                        return std::nullopt;
                    }

                    auto const nextStart = nl + 1;
                    auto const terminators = std::string_view{"\r\n\0\x03", 4};
                    auto nextEnd = text.find_first_of(terminators, nextStart);
                    if (nextEnd == std::string_view::npos) {
                        nextEnd = text.size();
                    }

                    auto const len = nextEnd - nextStart;
                    auto const maxIdx = len > 0 ? len - 1 : 0;
                    auto const idx = std::min<std::size_t>(pos.col - 1, maxIdx);

                    return pane->setpos({pos.line + 1, idx + 1, nextStart + idx});
                }
                return std::nullopt;

selection:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                return std::nullopt;

command:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Backspace) {
                    if (commandBuffer->view().size() > 12) {
                        return commandBuffer->remove(commandBuffer->view().size() - 1);
                    }
                }
                else if (c == core::input::CR || c == core::input::LF) {
                    commandBuffer->view().remove_prefix(12);

                    if (commandBuffer->view() == "w") {
                        this->mode = Mode::Navigation;
                        return this->manager.flush(this->panes.at(this->active)->getname());
                    }
                    else {
                        return "Unknown command";
                    }
                }
                else if (!std::iscntrl(c)) {
                    commandBuffer->append(commandBuffer->view().size(), static_cast<char>(c));
                }
                return std::nullopt;
            }

            auto draw() -> std::optional<std::string_view> override {
                std::optional<std::string_view> res = std::nullopt;

                this->screen = ftxui::Screen::Create(ftxui::Dimension::Full());
                WindowContext context {
                    .manager = this->manager,
                    .screen = this->screen,
                    .element = ftxui::text("No Buffer"),
                };

                if (this->panes.empty()) {
                    res = "No panes to draw.";
                }
                else {
                    res = this->panes.at(this->active)->draw(&context);
                }

                auto const cmdBuf = this->manager.get("<command_buffer>").value()->view();

                auto const body = ftxui::flex(
                    ftxui::vbox(
                        ftxui::flex(context.element),
                        ftxui::bgcolor(
                            res ? ftxui::Color::Red : ftxui::Color(ftxui::Color::Palette1::Default),
                            ftxui::text(res ? res.value() : cmdBuf)
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
                auto const half = static_cast<std::size_t>(std::max((this->screen.dimy() - 2) / 2, 0));
                auto const topLine = pos.line > half ? pos.line - half : 1;
                this->screen.CellAt(pos.col - 1, pos.line - topLine).inverted = true;
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
