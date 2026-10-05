export module core.input;

import std;

namespace core::input {
    export class Input {
        public:
            virtual auto poll() -> void = 0;
    };
}
