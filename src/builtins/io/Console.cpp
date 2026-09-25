#include "runtime/objects/EZObjects.h"
#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#else
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#endif
#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include "runtime/EZFuture.h"
#include "builtins/Builtins.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <string>

#ifndef _WIN32
static char posix_getch() {
    char buf = 0;
    struct termios old = {0};
    if (tcgetattr(STDIN_FILENO, &old) < 0) return 0;
    struct termios current = old;
    current.c_lflag &= ~(ICANON | ECHO);
    current.c_cc[VMIN] = 1;
    current.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSANOW, &current) < 0) return 0;
    if (read(STDIN_FILENO, &buf, 1) < 0) buf = 0;
    tcsetattr(STDIN_FILENO, TCSADRAIN, &old);
    return buf;
}
#endif

void registerConsoleBuiltins(RuntimeContext& interp) {
    auto clearFn = Value::makeNativeFunction("clear", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            system("cls");
#else
            std::cout << "\033[2J\033[H" << std::flush;
#endif
            return Value();
        });
    interp.defineGlobal("clear", clearFn);
    interp.defineGlobal("console_clear", clearFn);

    auto colorFn = Value::makeNativeFunction("color", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("color() expects a number code (0-15)", 0, ""); return Value(); }
            int code = (int)args[0].asNumber();
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, (WORD)code);
#else
            static const char* ansiColors[] = {
                "\033[30m", "\033[34m", "\033[32m", "\033[36m",
                "\033[31m", "\033[35m", "\033[33m", "\033[37m",
                "\033[90m", "\033[94m", "\033[92m", "\033[96m",
                "\033[91m", "\033[95m", "\033[93m", "\033[97m"
            };
            if (code >= 0 && code <= 15) {
                std::cout << ansiColors[code] << std::flush;
            }
#endif
            return Value();
        });
    interp.defineGlobal("color", colorFn);
    interp.defineGlobal("console_color", colorFn);

    auto resetFn = Value::makeNativeFunction("reset", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 7); 
#else
            std::cout << "\033[0m" << std::flush;
#endif
            return Value();
        });
    interp.defineGlobal("reset", resetFn);
    interp.defineGlobal("console_reset", resetFn);

    auto gotoxyFn = Value::makeNativeFunction("gotoxy", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) 
                { interp.runtimeError("gotoxy() expects two numbers (x, y)", 0, ""); return Value(); }
            int x = (int)args[0].asNumber();
            int y = (int)args[1].asNumber();
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(hConsole, pos);
#else
            std::cout << "\033[" << (y + 1) << ";" << (x + 1) << "H" << std::flush;
#endif
            return Value();
        });
    interp.defineGlobal("gotoxy", gotoxyFn);
    interp.defineGlobal("console_cursor_pos", gotoxyFn);

    auto getchFn = Value::makeNativeFunction("getch", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            int c = _getch();
            return Value(std::string(1, (char)c));
#else
            char c = posix_getch();
            return Value(std::string(1, c));
#endif
        });
    interp.defineGlobal("getch", getchFn);
    interp.defineGlobal("console_read_key", getchFn);

    auto titleFn = Value::makeNativeFunction("console_title", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.runtimeError("console_title() expects a string", 0, "");
                return Value();
            }
            const std::string title = args[0].asString();
#ifdef _WIN32
            SetConsoleTitleA(title.c_str());
#else
            std::cout << "\033]0;" << title << "\007" << std::flush;
#endif
            return Value();
        });
    interp.defineGlobal("console_title", titleFn);
    interp.defineGlobal("title", titleFn);

    auto hideCursorFn = Value::makeNativeFunction("console_hide_cursor", -1,
        [](RuntimeContext&, const std::vector<Value>& args) -> Value {
            bool hide = true;
            if (!args.empty() && args[0].isBool()) {
                hide = args[0].asBool();
            }
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_CURSOR_INFO info;
            if (GetConsoleCursorInfo(hConsole, &info)) {
                info.bVisible = !hide;
                SetConsoleCursorInfo(hConsole, &info);
            }
#else
            std::cout << (hide ? "\033[?25l" : "\033[?25h") << std::flush;
#endif
            return Value();
        });
    interp.defineGlobal("console_hide_cursor", hideCursorFn);

    auto sizeFn = Value::makeNativeFunction("console_size", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            int cols = 80;
            int rows = 24;
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            CONSOLE_SCREEN_BUFFER_INFO csbi;
            if (GetConsoleScreenBufferInfo(hConsole, &csbi)) {
                cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
                rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
            }
#else
            struct winsize w;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
                cols = w.ws_col;
                rows = w.ws_row;
            }
#endif
            return Value::makeArray({Value(cols), Value(rows)});
        });
    interp.defineGlobal("console_size", sizeFn);
}
