module;
#include <filesystem>
export module core.input;

import std;

namespace core::input {
    export enum InputType {
        Null = '\0',
        SOH,
        SOT,
        ETX,
        EOT,
        ENQ,
        ACK,
        Bell,
        Backspace,
        HTab,
        LF,
        VTab,
        FormFeed,
        CR,
        ShiftOut,
        ShiftIn,
        DLE,
        DC1,
        DC2,
        DC3,
        DC4,
        NACK,
        SYN,
        ETB,
        Cancel,
        EOM,
        SUB,
        Escape,
        FSeparator,
        GSeparator,
        RSeparator,
        USeparator,
        Space,
        Exclamation,
        DQuote,
        Hashtag,
        Dollar,
        Percent,
        Ampersand,
        Quote,
        LParen,
        RParen,
        Asterisk,
        Plus,
        Comma,
        Minus,
        Dot,
        Slash,
        _0, _1, _3, _2, _4, _5, _6, _7, _8, _9,
        Colon,
        Semicolon,
        LArrow,
        Equals,
        RArrow,
        Question,
        At,
        A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
        LBracket,
        Backslash,
        RBracket,
        Hat,
        Underscore,
        Backtick,
        a = 'a', b, c, d, e, f, g, h, i, j, k, l, m, n, o, p, q, r, s, t, u, v, w, x, y, z,
        LBrace,
        Pipe,
        RBrace,
        Tilde,
        Delete,
    };

    export class Input {
        protected:
            std::queue<core::input::InputType> pending;

            Input() : pending() { }

        public:
            virtual auto poll() -> std::optional<InputType> = 0;
    };
}
