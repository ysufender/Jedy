export module core.loop;

import core.window;
import core.input;

namespace loop {
    export auto mainLoop(window::IWindow window, input::Input input) -> void {
        while (true) {
            input.poll();
            window.draw();
        }
    }
}
