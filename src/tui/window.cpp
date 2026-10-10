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

auto computeTopLine(std::string_view const text, std::size_t const line, int const dimy) -> std::size_t {
    auto const ymax = static_cast<std::size_t>(std::max(dimy - 2, 0));
    auto const half = ymax / 2;
    auto const total = static_cast<std::size_t>(std::ranges::count(text, '\n')) + 1;
    auto const maxTop = total > ymax ? total - ymax + 1 : 1;
    return std::min(line > half ? line - half : 1, maxTop);
}

auto charClass(char const c) -> int {
    if (std::isspace(static_cast<unsigned char>(c))) {
        return 0;
    }
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') {
        return 1;
    }
    return 2;
}

auto positionAt(std::string_view const text, std::size_t const off) -> core::Position {
    auto const head = text.subview(0, off);
    auto const line = static_cast<std::size_t>(std::ranges::count(head, '\n')) + 1;
    auto const nl = head.rfind('\n');
    auto const lineStart = (nl == std::string_view::npos) ? 0 : nl + 1;
    return {line, off - lineStart + 1, off};
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
        "Command",
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

                auto text = found.value()->view();

                auto const topLine = computeTopLine(text, this->pos.line, context.screen.dimy());
                auto const ymax = static_cast<std::size_t>(std::max(context.screen.dimy() - 2, 0));

                std::size_t start = 0;
                if (topLine > 1) {
                    auto const nl = nthOccurrence(text, '\n', topLine - 1);
                    if (nl == std::string_view::npos) {
                        return "Cursor line is out of range.";
                    }
                    start = nl + 1;
                }
                text.remove_prefix(start);

                std::vector<ftxui::Element> vec;
                vec.reserve(ymax);
                vec.reserve(ymax);

                for (unsigned int i = 0; i < ymax; ++i) {
                    auto const nl = text.find('\n');
                    auto line = text.subview(0, nl);
                    if (!line.empty() && line.back() == '\r') {
                        line.remove_suffix(1);
                    }

                    vec.emplace_back(ftxui::text(std::format("{:>5}| {}", topLine + i, line)));

                    if (nl == std::string_view::npos) {
                        break;
                    }
                    text.remove_prefix(nl + 1);
                }
                context.element = ftxui::vbox(vec);

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
            core::Position selectionStart;

        public:
            static constexpr std::size_t DefaultCommandBufferSize = 255;

            std::optional<std::string_view> errBuf;

            Window(core::buffer::BufferManager& manager,
                   std::unique_ptr<core::input::Input>&& input)
                : core::window::Window(manager, std::move(input)),
                  rawmode(),
                  closing(false),
                  screen(ftxui::Screen::Create(ftxui::Dimension::Full())),
                  mode(Mode::Navigation),
                  cmdBuf("<command_buffer>"),
                  selectionStart(1, 1, 0) { }

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

                switch (this->mode) {
                    case Mode::Navigation: goto navigation;
                    case Mode::Input: goto input;
                    case Mode::Command: goto command;
                    case Mode::Selection: goto selection;
                }

selection:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::y) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);

                    return this->copySelection(true);
                }
                else if (c == core::input::P) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);

                    auto const res = this->deleteSelection(false);
                    if (res) {
                        return res;
                    }

                    return this->pasteCopyBuffer();
                }
                else if (c == core::input::D) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                    return this->deleteSelection(false);
                }
                else if (c == core::input::d) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                    return this->deleteSelection(true);
                }
                else if (c == core::input::w) {
                    return this->skipForward();
                }
                else if (c == core::input::b) {
                    return this->skipBackward();
                }
                else if (c == core::input::LBracket) {
                    return this->skipToBeginning();
                }
                else if (c == core::input::RBracket) {
                    return this->skipToEnd();
                }
                else if (c == core::input::h) {
                    return this->moveLeft();
                }
                else if (c == core::input::l) {
                    return this->moveRight();
                }
                else if (c == core::input::k) {
                    return this->moveUp();
                }
                else if (c == core::input::j) {
                    return this->moveDown();
                }
                return std::nullopt;

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
                            auto const nl = after.subview(0, end).rfind('\n');
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
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::p) {
                    return this->pasteCopyBuffer();
                }
                else if (c == core::input::v) {
                    this->selectionStart = this->panes.at(this->active)->getpos();
                    this->mode = Mode::Selection;
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::_0) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();
                    return pane->setpos({pos.line, 1, pos.off - pos.col + 1});
                }
                else if (c == core::input::Dollar) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    auto const pos = pane->getpos();
                    auto res = found.value()->view().subview(pos.off).find_first_of("\n\0");
                    if (res != std::string::npos) {
                        return pane->setpos({pos.line, pos.col + res, pos.off + res});
                    }
                }
                else if (c == core::input::w) {
                    return this->skipForward();
                }
                else if (c == core::input::b) {
                    return this->skipBackward();
                }
                else if (c == core::input::LBracket) {
                    return this->skipToBeginning();
                }
                else if (c == core::input::RBracket) {
                    return this->skipToEnd();
                }
                else if (c == core::input::a) {
                    this->mode = Mode::Input;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);

                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();

                    auto const maybeBuf = this->manager.get(pane->getname());

                    if (!maybeBuf) {
                        return "Failed to get buffer";
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
                    commandBuffer->assign(":");
                }
                else if (c == core::input::h) {
                    return this->moveLeft();
                }
                else if (c == core::input::l) {
                    return this->moveRight();
                }
                else if (c == core::input::k) {
                    return this->moveUp();
                }
                else if (c == core::input::j) {
                    return this->moveDown();
                }
                return std::nullopt;

command:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Backspace) {
                    if (commandBuffer->view().size() > 1) {
                        return commandBuffer->remove(commandBuffer->view().size() - 1);
                    }
                }
                else if (c == core::input::CR || c == core::input::LF) {
                    auto cmd = commandBuffer->view().substr(1);
                    std::optional<std::string_view> result = std::nullopt;
                    this->errBuf = "Ok";

                    while (!cmd.empty()) {
                        switch (cmd.front()) {
                            case 'w': {
                                auto const res = this->manager.flush(this->panes.at(this->active)->getname());
                                if (res) {
                                    this->errBuf = res;
                                    cmd = {};
                                    continue;
                                }
                                break;
                            }

                            case 'q': {
                                this->close();
                                break;
                            }

                            case 'e': {
                                cmd.remove_prefix(1);
                                while (!cmd.empty() && cmd.front() == ' ') {
                                    cmd.remove_prefix(1);
                                }

                                if (cmd.empty()) {
                                    this->errBuf = "Expected a file name";
                                    continue;
                                }

                                std::string_view const name{cmd};
                                cmd = {};

                                if (!this->manager.get(name)) {
                                    std::ifstream in { name.data(), std::ios::binary | std::ios::ate };
                                    auto const size = in.is_open() ? static_cast<std::size_t>(in.tellg()) : std::size_t{0};

                                    auto res = this->manager.buffer(name, size);
                                    if (res) {
                                        this->errBuf = res;
                                        continue;
                                    }

                                    if (in.is_open()) {
                                        in.seekg(0);
                                        auto const target = this->manager.get(name).value();

                                        char ch;
                                        while (in.get(ch)) {
                                            res = target->append(target->view().size(), ch);
                                            if (res) {
                                                this->errBuf = res;
                                                break;
                                            }
                                        }
                                        if (res) {
                                            continue;
                                        }
                                    }
                                }

                                auto const res = this->addPane(name);
                                if (res) {
                                    this->errBuf = res;
                                }
                                continue;
                            }

                            default: {
                                result = "Unknown command";
                                cmd = {};
                                continue;
                            }
                        }

                        cmd.remove_prefix(1);
                    }

                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                    return result;
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
                        ftxui::hbox(
                            ftxui::text(cmdBuf),
                            ftxui::flex(ftxui::align_right(ftxui::text(res.value_or(this->errBuf.value_or("Ok")))))
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
                auto const& activePane = this->panes.at(this->active);
                auto const pos = activePane->getpos();
                if (auto const buf = this->manager.get(activePane->getname())) {
                    auto const topLine = computeTopLine(buf.value()->view(), pos.line, this->screen.dimy());

                    if (this->mode == Mode::Selection) {
                        auto const text = buf.value()->view();
                        auto const lo = std::min(pos.off, this->selectionStart.off);
                        auto const hi = std::max(pos.off, this->selectionStart.off);
                        auto const ymax = static_cast<std::size_t>(std::max(this->screen.dimy() - 2, 0));

                        auto const start = positionAt(text, lo);
                        auto line = start.line;
                        auto col = start.col;

                        for (auto off = lo; off < hi && off < text.size(); ++off) {
                            auto const ch = text[off];
                            if (ch == '\n') {
                                ++line;
                                col = 1;
                                continue;
                            }
                            if (ch != '\r' && line >= topLine && line - topLine < ymax) {
                                this->screen.CellAt(static_cast<int>(col) + 6, static_cast<int>(line - topLine)).inverted = true;
                            }
                            ++col;
                        }
                    }
                    else {
                        this->screen.CellAt(static_cast<int>(pos.col) + 6, static_cast<int>(pos.line - topLine)).inverted = true;
                    }
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

        private:
            auto skipForward() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const pos = pane->getpos();

                auto const maybeBuf = this->manager.get(pane->getname());
                if (!maybeBuf) {
                    return "Failed to get buffer";
                }

                auto const view = maybeBuf.value()->view();
                auto const terminators = std::string_view{"\0\x03", 2};
                auto const end = std::min(view.find_first_of(terminators), view.size());
                auto const text = view.subview(0, end);

                auto off = pos.off;
                if (off >= end) {
                    return std::nullopt;
                }

                auto const cls = charClass(text[off]);
                if (cls != 0) {
                    while (off < end && charClass(text[off]) == cls) {
                        off++;
                    }
                }
                while (off < end && charClass(text[off]) == 0) {
                    off++;
                }

                return pane->setpos(positionAt(text, off));
            }

            auto skipBackward() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const pos = pane->getpos();

                auto const maybeBuf = this->manager.get(pane->getname());
                if (!maybeBuf) {
                    return "Failed to get buffer";
                }

                auto const view = maybeBuf.value()->view();
                auto const terminators = std::string_view{"\0\x03", 2};
                auto const end = std::min(view.find_first_of(terminators), view.size());
                auto const text = view.subview(0, end);

                auto off = std::min<std::size_t>(pos.off, end);

                while (off > 0 && charClass(text[off - 1]) == 0) {
                    off--;
                }
                if (off > 0) {
                    auto const cls = charClass(text[off - 1]);
                    while (off > 0 && charClass(text[off - 1]) == cls) {
                        off--;
                    }
                }

                return pane->setpos(positionAt(text, off));
            }

            auto skipToBeginning() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                return pane->setpos({1, 1, 0});
            }

            auto skipToEnd() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);

                auto const maybeBuf = this->manager.get(pane->getname());
                if (!maybeBuf) {
                    return "Failed to get buffer";
                }

                auto const view = maybeBuf.value()->view();
                auto const terminators = std::string_view{"\0\x03", 2};
                auto const end = std::min(view.find_first_of(terminators), view.size());
                auto const text = view.subview(0, end);

                auto const line = static_cast<std::size_t>(std::ranges::count(text, '\n')) + 1;
                auto const nl = text.rfind('\n');
                auto const lineStart = (nl == std::string_view::npos) ? 0 : nl + 1;

                return pane->setpos({line, end - lineStart + 1, end});
            }

            auto moveLeft() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const pos = pane->getpos();

                if (pos.col > 1) {
                    return pane->setpos({pos.line, pos.col - 1, pos.off - 1});
                }

                return std::nullopt;
            }

            auto moveRight() -> std::optional<std::string_view> {
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
                    return pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                }

                return std::nullopt;
            }

            auto moveUp() -> std::optional<std::string_view> {
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

                auto const nl = text.subview(0, lineStart - 1).rfind('\n');
                auto const prevStart = (nl == std::string_view::npos) ? 0 : nl + 1;
                auto const crlf = lineStart >= 2 && text[lineStart - 2] == '\r';
                auto const prevEnd = lineStart - 1 - (crlf ? 1 : 0);
                auto const len = prevEnd - prevStart;
                auto const maxIdx = len > 0 ? len - 1 : 0;
                auto const idx = std::min<std::size_t>(pos.col - 1, maxIdx);

                return pane->setpos({pos.line - 1, idx + 1, prevStart + idx});
            }

            auto moveDown() -> std::optional<std::string_view> {
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

            auto copySelection(bool const gostart) -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const buffer = this->manager.get(pane->getname());
                if (!buffer) {
                    return "Failed to get current buffer.";
                }

                auto const pos = pane->getpos();
                auto const start = std::min(this->selectionStart.off, pos.off);
                auto const end = std::max(this->selectionStart.off, pos.off);
                auto const selected = buffer.value()->view().subview(start, end - start);

                if (gostart && this->selectionStart.off < pos.off) {
                    pane->setpos(this->selectionStart);
                }

                auto const copyBuf = this->manager.get("<copy>").value();
                return copyBuf->assign(selected);
            }

            auto deleteSelection(bool const copy) -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const buffer = this->manager.get(pane->getname());
                if (!buffer) {
                    return "Failed to get current buffer.";
                }

                auto const pos = pane->getpos();
                auto const start = std::min(this->selectionStart.off, pos.off);
                auto const end = std::max(this->selectionStart.off, pos.off);
                auto const selected = buffer.value()->view().subview(start, end - start);

                if (copy) {
                    auto const res = this->copySelection(false);
                    if (res) {
                        return res;
                    }
                }

                for (std::size_t i = 0; i < selected.size(); i++) {
                    auto const res = buffer.value()->remove(start);
                    if (res) {
                        return res;
                    }
                }

                if (this->selectionStart.off < pos.off) {
                    pane->setpos(this->selectionStart);
                }

                return std::nullopt;
            }

            auto pasteCopyBuffer() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const buffer = this->manager.get(pane->getname());
                if (!buffer) {
                    return "Failed to get current buffer";
                }

                auto const copyBuf = this->manager.get("<copy>").value()->view();
                for (auto const ch : copyBuf) {
                    auto res = buffer.value()->append(pane->getpos().off, ch);
                    if (res) {
                        return res;
                    }

                    auto const pos = pane->getpos();
                    res = pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                    if (res) {
                        return res;
                    }
                }

                return std::nullopt;
            }
    };
}
