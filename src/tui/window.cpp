export module tui.window;

import std;

import ftxui;

import core.window;
import core.buffer.manager;

namespace tui::window {
    class Pane : public core::window::Pane {
    };

    export template<int MaxPane>
    class Window {
    };
}
