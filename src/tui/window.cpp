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
    if (std::isspace(static_cast<unsigned char>(c))) { return 0; }
    if (std::isalnum(static_cast<unsigned char>(c)) || c == '_') { return 1; }
    return 2;
}

auto lineEnd(std::string_view const text, std::size_t const off) -> std::size_t {
    auto const terminators = std::string_view{"\n\0\x03", 3};
    return std::min(text.find_first_of(terminators, off), text.size());
}

auto indentAt(std::string_view const text, std::size_t const off) -> std::size_t {
    auto const start = off - (off > 0 ? [&] {
        auto const nl = text.subview(0, off).rfind('\n');
        return nl == std::string_view::npos ? off : off - (nl + 1);
    }() : 0);
    auto const end = lineEnd(text, off);
    auto const line = text.subview(start, end - start);
    return std::min(line.find_first_not_of(' '), line.size());
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
        Search,
    };

    constexpr std::string_view ModeStr[] = {
        "Navigation",
        "Input",
        "Selection",
        "Command",
        "Search",
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

                for (unsigned int i = 0; i < ymax; ++i) {
                    auto const nl = text.find('\n');
                    auto line = text.subview(0, nl);
                    
                    auto const marked = std::find_if(this->marks.begin(), this->marks.end(), [&](core::Position const item) -> bool { return item.line == topLine + i; }) != this->marks.end();

                    vec.emplace_back(ftxui::text(std::format("{:>5}|{}{}", topLine + i, marked ? '*' : ' ', line)));

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
            
            auto mark() -> std::optional<std::string_view> override {
                try {
                    auto const iter = std::find_if(this->marks.begin(), this->marks.end(), [&](core::Position const item) -> bool { return item.line == this->pos.line; });
                    if (iter == this->marks.end()) {
                        this->marks.emplace_back(this->pos);
                    }
                    else {
                        this->marks.erase(iter);
                    }
                    
                    return std::nullopt;
                }
                catch (std::exception&) {
                    return "Failed to mark line";
                }
            }
            
            auto goMark(bool const backwards) -> std::optional<std::string_view> override {
                if (this->marks.empty()) {
                    return "No marks on current buffer.";
                }
            
                if (backwards) {
                    if (this->lastMark == 0) {
                        this->lastMark = this->marks.size() - 1;
                    }
                    else {
                        this->lastMark--;
                    }
                }
                else {
                    if (this->lastMark + 1 >= this->marks.size()) {
                        this->lastMark = 0;
                    }
                    else {
                        this->lastMark++;
                    }
                }
                
                return this->setpos(this->marks.at(this->lastMark));
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

            auto removePane(unsigned int const pane) -> std::optional<std::string_view> override {
                if (pane >= this->panes.size()) {
                    return "Pane out of range.";
                }

                this->panes.erase(this->panes.begin() + pane);
                return std::nullopt;
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

                switch (this->mode) {
                    case Mode::Navigation: goto navigation;
                    case Mode::Input: goto input;
                    case Mode::Command: goto command;
                    case Mode::Selection: goto selection;
                    case Mode::Search: goto search;
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
                else if (c == core::input::RArrow) {
                    return this->shiftSelection(true);
                }
                else if (c == core::input::LArrow) {
                    return this->shiftSelection(false);
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
                    auto const end = lineEnd(found.value()->view(), pos.off);
                    return pane->setpos({pos.line, pos.col + (end - pos.off), end});
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
                else if (c == core::input::LBrace) {
                    return this->skipBlockBackward();
                }
                else if (c == core::input::RBrace) {
                    return this->skipBlockForward();
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
                            auto const res = found.value()->remove(pos.off - 1);
                            if (res) { return res; }

                            auto const end = pos.off - 1;
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

                    return found.value()->remove(pos.off);
                }
                else if (c == core::input::HTab) {
                    auto& pane = this->panes.at(this->active);
                    auto const found = this->manager.get(pane->getname());
                    if (!found) {
                        return "Failed to get current buffer.";
                    }

                    for (int i = 0; i < 4; i++) {
                        auto const pos = pane->getpos();
                        auto res = found.value()->append(pos.off, ' ');
                        if (res) { return res; }
                        res = pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                        if (res) { return res; }
                    }
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
                        auto res = found.value()->append(pos.off, '\n');
                        if (res) {
                            return res;
                            }
                    
                    auto const lineStart = found.value()->view().subview(pos.off - pos.col + 1);
                        auto const indent = lineStart.find_first_not_of(' ');
                    
                    for (std::size_t i = 0; i < indent; i++) {
                            res = found.value()->append(pos.off + 1 + i, ' ');
                                if (res) {
                                    return res;
                                    }
                            }
                    
                    return pane->setpos({pos.line + 1, indent + 1, pos.off + 1 + indent});
                }
                return std::nullopt;

navigation:
                if (c == core::input::i) {
                    this->mode = Mode::Input;
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::v) {
                    this->selectionStart = this->panes.at(this->active)->getpos();
                    this->mode = Mode::Selection;
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Colon) {
                    this->mode = Mode::Command;
                    commandBuffer->assign(":");
                }
                else if (c == core::input::Slash) {
                    this->mode = Mode::Search;
                    commandBuffer->assign("/");
                }
                else if (c == core::input::RBrace) {
                    return this->skipBlockForward();
                }
                else if (c == core::input::LBrace) {
                    return this->skipBlockBackward();
                }
                else if (c == core::input::m) {
                    return this->panes.at(this->active)->mark();
                }
                else if (c == core::input::n) {
                    return this->panes.at(this->active)->goMark(false);
                }
                else if (c == core::input::N) {
                    return this->panes.at(this->active)->goMark(true);
                }
                else if (c == core::input::s) {
                    std::optional<std::string_view> res;
                    if (static_cast<unsigned int>(this->active + 1) >= this->panes.size()) {
                        res = this->switchPane(0);
                    }
                    else {
                        res = this->switchPane(this->active + 1);
                    }
                    return res;
                }
                else if (c == core::input::S) {
                    std::optional<std::string_view> res;
                    if (this->active - 1 < 0) {
                        res = this->switchPane(this->panes.size() - 1);
                    }
                    else {
                        res = this->switchPane(this->active - 1);
                    }
                    return res;
                }
                else if (c == core::input::O) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();
                    auto const buffer = this->manager.get(pane->getname());
                    if (!buffer) {
                        return "Failed to get current buffer.";
                    }

                    auto const lineStart = pos.off - (pos.col - 1);
                    auto const indent = indentAt(buffer.value()->view(), pos.off);

                    auto res = buffer.value()->append(lineStart, '\n');
                    if (res) {
                        return res;
                    }

                    for (std::size_t i = 0; i < indent; i++) {
                        res = buffer.value()->append(lineStart + i, ' ');
                        if (res) {
                            return res;
                        }
                    }

                    res = pane->setpos({pos.line, indent + 1, lineStart + indent});
                    if (res) {
                        return res;
                    }

                    this->mode = Mode::Input;
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::o) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();
                    auto const buffer = this->manager.get(pane->getname());
                    if (!buffer) {
                        return "Failed to get current buffer.";
                    }

                    auto const view = buffer.value()->view();
                    auto const end = lineEnd(view, pos.off);
                    auto const hasNewline = end < view.size() && view[end] == '\n';
                    auto const insertAt = hasNewline ? end + 1 : end;
                    auto const nextStart = hasNewline ? insertAt : insertAt + 1;
                    auto const indent = indentAt(view, pos.off);

                    auto res = buffer.value()->append(insertAt, '\n');
                    if (res) {
                        return res;
                    }

                    for (std::size_t i = 0; i < indent; i++) {
                        res = buffer.value()->append(nextStart + i, ' ');
                        if (res) {
                            return res;
                        }
                    }

                    res = pane->setpos({pos.line + 1, indent + 1, nextStart + indent});
                    if (res) {
                        return res;
                    }

                    this->mode = Mode::Input;
                    return commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                }
                else if (c == core::input::Y) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();
                    auto const buffer = this->manager.get(pane->getname());
                    if (!buffer) {
                        return "Failed to get current buffer.";
                    }

                    auto const lineStart = pos.off - (pos.col - 1);
                    auto const end = lineEnd(buffer.value()->view(), pos.off);

                    this->selectionStart = {pos.line, 1, lineStart};
                    auto res = pane->setpos({pos.line, end - lineStart + 1, end});
                    if (res) {
                        return res;
                    }

                    res = this->copySelection(false);
                    if (res) {
                        return res;
                    }

                    return pane->setpos(pos);
                }
                else if (c == core::input::D) {
                    auto& pane = this->panes.at(this->active);
                    auto const pos = pane->getpos();
                    auto const buffer = this->manager.get(pane->getname());
                    if (!buffer) {
                        return "Failed to get current buffer.";
                    }

                    auto const view = buffer.value()->view();
                    auto const lineStart = pos.off - (pos.col - 1);
                    auto const end = lineEnd(view, pos.off);

                    auto from = lineStart;
                    auto to = end;
                    if (end < view.size() && view[end] == '\n') {
                        to = end + 1;
                    }
                    else if (lineStart >= 1 && view[lineStart - 1] == '\n') {
                        from = lineStart - 1;
                    }

                    this->selectionStart = positionAt(view, from);
                    auto const res = pane->setpos(positionAt(view, to));
                    if (res) {
                        return res;
                    }

                    return this->deleteSelection(true);
                }
                else if (c == core::input::J) {
                    auto& pane = this->panes.at(this->active);
                    auto const buffer = this->manager.get(pane->getname());
                    if (!buffer) { return "Failed to get current buffer."; }
                    
                    auto const pos = pane->getpos();
                    auto const nl = buffer.value()->view().subview(pos.off).find("\n");
                    if (nl != std::string::npos) {
                        auto res = buffer.value()->remove(nl + pos.off);
                        if (res) { return res; }
                        
                        while (buffer.value()->view()[pos.off + nl] == ' ') {
                            res = buffer.value()->remove(pos.off + nl);
                            if (res) { return res; }
                        }
                        
                        res = buffer.value()->append(pos.off + nl, ' ');
                        if (res) { return res; }
                    }
                }
                else if (c == core::input::p) {
                    return this->pasteCopyBuffer();
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
                    auto const end = lineEnd(found.value()->view(), pos.off);
                    return pane->setpos({pos.line, pos.col + (end - pos.off), end});
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
                        && next.value() != '\n'
                        && next.value() != '\0'
                        && next.value() != core::input::ETX) {
                        pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                    }
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

search:
                if (c == core::input::Escape) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                    return std::nullopt;
                }

                if (c == core::input::Backspace) {
                    if (commandBuffer->view().size() > 1) {
                        return commandBuffer->remove(commandBuffer->view().size() - 1);
                    }
                    return std::nullopt;
                }

                if (c == core::input::CR || c == core::input::LF) {
                    this->mode = Mode::Navigation;
                    commandBuffer->assign(ModeStr[static_cast<int>(this->mode)]);
                    return std::nullopt;
                }

                if (c == core::input::HTab) {
                    return this->search(true);
                }

                if (std::iscntrl(c)) {
                    return std::nullopt;
                }

                commandBuffer->append(commandBuffer->view().size(), static_cast<char>(c));
                return this->search(false);

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

                            case 'k': {
                                cmd = {};

                                if (this->panes.size() == 1) {
                                    this->close();
                                    continue;
                                }

                                auto const bufName = this->panes.at(this->active)->getname();
                                auto res = this->manager.remove(bufName);
                                if (res) {
                                    this->errBuf = res;
                                    continue;
                                }

                                res = this->removePane(this->active);
                                if (res) {
                                    this->errBuf = res;
                                    continue;
                                }

                                this->active = std::max(this->active - 1, 0);
                                continue;
                            }

                            case 'p': {
                                cmd.remove_prefix(1);
                                while (!cmd.empty() && cmd.front() == ' ') {
                                    cmd.remove_prefix(1);
                                }

                                if (cmd.empty()) {
                                    this->errBuf = "Expected pane number";
                                    continue;
                                }

                                auto const paneNum = cmd;
                                cmd = {};

                                int num;
                                auto const [ptr, ec] = std::from_chars(paneNum.data(), paneNum.data() + paneNum.size(), num);
                                if (ec != std::errc{} || ptr != paneNum.data() + paneNum.size()) {
                                    this->errBuf = "Invalid format.";
                                    continue;
                                }

                                auto const res = this->switchPane(num);
                                if (res) {
                                    this->errBuf = res;
                                }
                                continue;
                            }

                            case 'l': {
                                cmd.remove_prefix(1);
                                while (!cmd.empty() && cmd.front() == ' ') {
                                    cmd.remove_prefix(1);
                                }
                                
                                if (cmd.empty()) {
                                    this->errBuf = "Expected line number";
                                    continue;
                                }

                                auto const lineNum = cmd;
                                cmd = {};
                                
                                int num;
                                auto const [ptr, ec] = std::from_chars(lineNum.data(), lineNum.data() + lineNum.size(), num);
                                if (ec != std::errc{} || ptr != lineNum.data() + lineNum.size()) {
                                    this->errBuf = "Invalid format.";
                                    continue;
                                }

                                auto& pane = this->panes.at(this->active);
                                auto const buffer = this->manager.get(pane->getname());
                                if (!buffer) {
                                    this->errBuf = "Failed to get current buffer.";
                                    continue;
                                }

                                auto const off = nthOccurrence(buffer.value()->view(), '\n', num - 1);
                                if (off == std::string::npos) {
                                    this->errBuf = "No such line";
                                    continue;
                                }

                                auto const res = pane->setpos({static_cast<std::size_t>(num), 1, off + 1});
                                if (res) {
                                    this->errBuf = res;
                                }
                                continue;
                            }

                            case 'e': {
                                cmd.remove_prefix(1);
                                while (!cmd.empty() && cmd.front() == ' ') {
                                    cmd.remove_prefix(1);
                                }

                                if (cmd.empty()) {
                                    auto const res = this->reopenFileToBuffer(this->panes.at(this->active)->getname()); 
                                    if (res) {
                                        this->errBuf = res;
                                        continue;
                                    }
                                }
                                
                                std::string_view const name{cmd};
                                cmd = {};

                                if (!this->manager.get(name)) {
                                    auto const res = this->openFileToBuffer(name);
                                    if (res) {
                                        this->errBuf = res;
                                        continue;
                                    }
                                }
                                else {
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
                else if (c == core::input::HTab) {
                    for (int i = 0; i < 4; i++) {
                        commandBuffer->append(commandBuffer->view().size(), ' ');
                    }
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

                std::string paneName = "<none>";
                std::size_t paneLine = 0;
                std::size_t paneCol = 0;

                if (this->panes.empty()) {
                    res = "No panes to draw.";
                }
                else {
                    auto const& current = this->panes.at(this->active);
                    res = current->draw(&context);
                    paneName = std::string{current->getname()};
                    paneLine = current->getpos().line;
                    paneCol = current->getpos().col;
                }

                auto const cmdLine = this->manager.get("<command_buffer>").value()->view();

                auto const body = ftxui::flex(
                    ftxui::vbox(
                        ftxui::flex(context.element),
                        ftxui::hbox(
                            ftxui::text(cmdLine),
                            ftxui::flex(ftxui::align_right(ftxui::text(res.value_or(this->errBuf.value_or("Ok")))))
                        ),
                        ftxui::color(
                            ftxui::Color::Black,
                            ftxui::bgcolor(
                                ftxui::Color::Green,
                                ftxui::hbox(
                                    ftxui::text(paneName),
                                    ftxui::text(":"),
                                    ftxui::text(std::to_string(paneLine)),
                                    ftxui::text(":"),
                                    ftxui::text(std::to_string(paneCol))
                                )
                            )
                        )
                    )
                );

                ftxui::Render(this->screen, body);

                if (!this->panes.empty()) {
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
                                if (line >= topLine && line - topLine < ymax) {
                                    this->screen.CellAt(static_cast<int>(col) + 6, static_cast<int>(line - topLine)).inverted = true;
                                }
                                ++col;
                            }
                        }
                        else {
                            this->screen.CellAt(static_cast<int>(pos.col) + 6, static_cast<int>(pos.line - topLine)).inverted = true;
                        }
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

                if (delta < 0 || static_cast<std::size_t>(delta) >= this->panes.size()) {
                    return "Pane out of range.";
                }

                this->active = delta;
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
            } //end skipForward

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
            } //end skipBackward

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
            } //end skipToEnd

            auto moveLeft() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const pos = pane->getpos();

                if (pos.col > 1) {
                    return pane->setpos({pos.line, pos.col - 1, pos.off - 1});
                }

                return std::nullopt;
            } //end moveLeft

            auto moveRight() -> std::optional<std::string_view> {
                auto& pane = this->panes.at(this->active);
                auto const pos = pane->getpos();

                auto const maybeBuf = this->manager.get(pane->getname());

                if (!maybeBuf) {
                    return std::nullopt;
                }

                auto const next = maybeBuf.value()->get(pos.off);
                if (next
                    && next.value() != '\n'
                    && next.value() != '\0'
                    && next.value() != core::input::ETX) {
                    return pane->setpos({pos.line, pos.col + 1, pos.off + 1});
                }

                return std::nullopt;
            } //end moveRight

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
                auto const prevEnd = lineStart - 1;
                auto const len = prevEnd - prevStart;
                auto const maxIdx = len > 0 ? len - 1 : 0;
                auto const idx = std::min<std::size_t>(pos.col - 1, maxIdx);

                return pane->setpos({pos.line - 1, idx + 1, prevStart + idx});
            } //end moveUp

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
                auto const terminators = std::string_view{"\n\0\x03", 3};
                auto nextEnd = text.find_first_of(terminators, nextStart);
                if (nextEnd == std::string_view::npos) {
                    nextEnd = text.size();
                }

                auto const len = nextEnd - nextStart;
                auto const maxIdx = len > 0 ? len - 1 : 0;
                auto const idx = std::min<std::size_t>(pos.col - 1, maxIdx);

                return pane->setpos({pos.line + 1, idx + 1, nextStart + idx});
            } //end moveDown

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
            } //end copySelection

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
            } //end deleteSelection

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
                    auto const newPos =
                        ch == '\n'
                        ? core::Position{pos.line + 1, 1, pos.off + 1}
                        : core::Position{pos.line, pos.col + 1, pos.off + 1};

                    res = pane->setpos(newPos);
                    if (res) {
                        return res;
                    }
                }

                return std::nullopt;
            } //end pasteCopyBuffer

            auto search(bool const next) -> std::optional<std::string_view> {
                auto const commandBuffer = this->manager.get("<command_buffer>").value();
                auto const query = commandBuffer->view().substr(1);
                if (query.empty()) {
                    return std::nullopt;
                }

                auto const direction = query.front();
                if (direction != 'f' && direction != 'b') {
                    return "Search starts with f or b.";
                }

                auto const pattern = query.substr(1);
                if (pattern.empty()) {
                    return std::nullopt;
                }

                auto& pane = this->panes.at(this->active);
                auto const found = this->manager.get(pane->getname());
                if (!found) {
                    return "Failed to get current buffer.";
                }

                auto const view = found.value()->view();
                auto const terminators = std::string_view{"\0\x03", 2};
                auto const end = std::min(view.find_first_of(terminators), view.size());
                auto const text = view.subview(0, end);

                auto const pos = pane->getpos().off;
                std::size_t match = std::string_view::npos;

                if (direction == 'f') {
                    match = text.find(pattern, next ? pos + 1 : pos);
                }
                else if (!next) {
                    match = text.rfind(pattern, pos);
                }
                else if (pos > 0) {
                    match = text.rfind(pattern, pos - 1);
                }

                if (match == std::string_view::npos) {
                    return "Not found";
                }

                return pane->setpos(positionAt(text, match));
            } //end search

            auto skipBlockForward() -> std::optional<std::string_view> {
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

                auto const isBlank = [&](std::size_t const s) {
                    auto const e = std::min(text.find('\n', s), text.size());
                    return text.subview(s, e - s).find_first_not_of(' ') == std::string_view::npos;
                };

                auto const nextLine = [&](std::size_t const s) -> std::optional<std::size_t> {
                    auto const e = text.find('\n', s);
                    if (e == std::string_view::npos) {
                        return std::nullopt;
                    }
                    return e + 1;
                };

                std::optional<std::size_t> cur = pos.off - (pos.col - 1);
                while (cur && isBlank(*cur)) {
                    cur = nextLine(*cur);
                }
                while (cur && !isBlank(*cur)) {
                    cur = nextLine(*cur);
                }

                return pane->setpos(positionAt(text, cur.value_or(text.size())));
            } //end skipBlockForward

            auto skipBlockBackward() -> std::optional<std::string_view> {
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

                auto const isBlank = [&](std::size_t const s) {
                    auto const e = std::min(text.find('\n', s), text.size());
                    return text.subview(s, e - s).find_first_not_of(' ') == std::string_view::npos;
                };

                auto const prevLine = [&](std::size_t const s) -> std::optional<std::size_t> {
                    if (s == 0) {
                        return std::nullopt;
                    }
                    auto const nl = text.subview(0, s - 1).rfind('\n');
                    return nl == std::string_view::npos ? 0 : nl + 1;
                };

                std::optional<std::size_t> cur = std::min(pos.off - (pos.col - 1), text.size());
                while (cur && isBlank(*cur)) {
                    cur = prevLine(*cur);
                }
                while (cur && !isBlank(*cur)) {
                    cur = prevLine(*cur);
                }

                return pane->setpos(positionAt(text, cur.value_or(0)));
            } //end skipBlockBackward
            
            auto openFileToBuffer(std::string_view const name) -> std::optional<std::string_view> {
                std::ifstream in { std::string{name}, std::ios::binary | std::ios::ate };
                auto const size = in.is_open() ? static_cast<std::size_t>(in.tellg()) : std::size_t{0};

                auto res = this->manager.buffer(name, size);
                if (res) {
                    return res;
                }

                if (in.is_open()) {
                    in.seekg(0);
                    auto const target = this->manager.get(name).value();

                    char ch;
                    while (in.get(ch)) {
                        res = target->append(target->view().size(), ch);
                        if (res) {
                            return res;
                        }
                    }
                    if (res) {
                        return res;
                    }
                }
                else {
                    return "Failed to open file";
                }
                
                return std::nullopt;
            } //end openFileToBuffer
            
            auto reopenFileToBuffer(std::string_view const name) -> std::optional<std::string_view> {
                std::ifstream in { std::string{name}, std::ios::binary | std::ios::ate };

                if (in.is_open()) {
                    in.seekg(0);
                    auto const target = this->manager.get(name).value();

                    char ch;
                    std::optional<std::string_view> res { std::nullopt };
                    while (in.get(ch)) {
                        res = target->append(target->view().size(), ch);
                        if (res) {
                            return res;
                        }
                    }
                    if (res) {
                        return res;
                    }
                }
                else {
                    return "Failed to open file";
                }
                
                return std::nullopt;
            } //end reopenFileToBuffer

            auto shiftSelection(bool const indent) -> std::optional<std::string_view> {
                constexpr std::size_t Width = 4;

                auto& pane = this->panes.at(this->active);
                auto const buffer = this->manager.get(pane->getname());
                if (!buffer) {
                    return "Failed to get current buffer.";
                }

                auto const pos = pane->getpos();
                auto const lo = std::min(this->selectionStart.off, pos.off);
                auto const hi = std::max(this->selectionStart.off, pos.off);

                std::vector<std::pair<std::size_t, std::size_t>> edits;
                {
                    auto const view = buffer.value()->view();

                    auto const nl = view.subview(0, lo).rfind('\n');
                    auto const firstStart = (nl == std::string_view::npos) ? 0 : nl + 1;

                    std::vector<std::size_t> starts{firstStart};
                    for (auto i = lo; i < hi && i < view.size(); ++i) {
                        if (view[i] == '\n') {
                            starts.push_back(i + 1);
                        }
                    }

                    for (auto const s : starts) {
                        std::size_t count = 0;
                        if (indent) {
                            auto const empty = s >= view.size()
                                || view[s] == '\n'
                                || view[s] == '\0'
                                || view[s] == core::input::ETX;
                            count = empty ? 0 : Width;
                        }
                        else {
                            while (count < Width && s + count < view.size() && view[s + count] == ' ') {
                                ++count;
                            }
                        }

                        if (count > 0) {
                            edits.emplace_back(s, count);
                        }
                    }
                }

                auto const remap = [&](std::size_t const off) -> std::size_t {
                    std::ptrdiff_t shift = 0;
                    for (auto const& [s, count] : edits) {
                        if (indent) {
                            if (s <= off) {
                                shift += static_cast<std::ptrdiff_t>(count);
                            }
                        }
                        else if (off >= s + count) {
                            shift -= static_cast<std::ptrdiff_t>(count);
                        }
                        else if (off > s) {
                            shift -= static_cast<std::ptrdiff_t>(off - s);
                        }
                    }
                    return static_cast<std::size_t>(static_cast<std::ptrdiff_t>(off) + shift);
                };

                auto const newStartOff = remap(this->selectionStart.off);
                auto const newCursorOff = remap(pos.off);

                for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
                    auto const [s, count] = *it;
                    for (std::size_t i = 0; i < count; ++i) {
                        auto const res = indent
                            ? buffer.value()->append(s, ' ')
                            : buffer.value()->remove(s);
                        if (res) {
                            return res;
                        }
                    }
                }

                auto const view = buffer.value()->view();
                this->selectionStart = positionAt(view, newStartOff);
                return pane->setpos(positionAt(view, newCursorOff));
            } //end shiftSelection
    };
}
