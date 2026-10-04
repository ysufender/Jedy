module;
#include <termios.h>
#include <unistd.h>
#include <signal.h>

export module core.rawmode;

import std;

namespace core {
    export class RawMode {
        static inline termios saved{};
        static inline bool savedValid = false;

        static void emit(char const* s) {
            (void)!::write(STDOUT_FILENO, s, std::strlen(s));
        }

        static void onSignal(int sig) {
            if (savedValid) {
                emit("\x1b[?25h\x1b[?1049l");
                tcsetattr(STDIN_FILENO, TCSAFLUSH, &saved);
            }
            _exit(128 + sig);
        }

        termios orig{};
        bool active = false;

        public:
            RawMode() {
                if (tcgetattr(STDIN_FILENO, &orig) == -1) return;
                saved = orig;
                savedValid = true;

                termios term = orig;
                term.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
                term.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
                term.c_cc[VMIN]  = 1;
                term.c_cc[VTIME] = 0;
                active = tcsetattr(STDIN_FILENO, TCSAFLUSH, &term) == 0;
                if (!active) return;

                emit("\x1b[?1049h\x1b[?25l\x1b[2J");

                struct sigaction sa{};
                sa.sa_handler = onSignal;
                sigemptyset(&sa.sa_mask);
                for (int s : {SIGINT, SIGTERM, SIGHUP, SIGQUIT}) {
                    sigaction(s, &sa, nullptr);
                }
            }
            ~RawMode() {
                if (active) {
                    emit("\x1b[?25h\x1b[?1049l");
                    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig);
                    savedValid = false;
                }
            }
            RawMode(RawMode const&) = delete;
            auto operator=(RawMode const&) -> RawMode& = delete;
    };
}
