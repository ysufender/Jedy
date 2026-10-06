export module core.input;

import std;

namespace core::input {
    export enum InputType {
        Terminate = 0x03,
        Escape = 0x1b,
        A = 'a', B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        a = 'a', b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
        _0 = '0', _1, _2, _4, _5, _6, _7, _8, _9,
    };

    export class Input {
        protected:
            std::queue<core::input::InputType> pending;

            Input() : pending() { }

        public:
            virtual auto poll() -> std::optional<InputType> = 0;
    };
}
