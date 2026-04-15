#include "../Builtins.h"
#include "../Interpreter.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>
#include <vector>
#include <string>

#ifdef _WIN32
#include <windows.h>
#include <conio.h>
#include <shellapi.h>
#endif

void registerSysBuiltins(Interpreter& interp) {
    interp.defineGlobal("clock", Value::makeNativeFunction("clock", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            return Value((double)ms);
        }));

    interp.defineGlobal("stop", Value::makeNativeFunction("stop", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("stop() expects number", 0, ""); return Value(); }
            std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asNumber()));
            return Value();
        }));

    interp.defineGlobal("spawn", Value::makeNativeFunction("spawn", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isCallable()) { interp.runtimeError("spawn() expects function", 0, ""); return Value(); }
            Value func = args[0];
            std::vector<Value> fnArgs(args.begin() + 1, args.end());
            auto globalEnv = interp.getGlobalEnv();
            
            std::shared_future<Value> fut = std::async(std::launch::async, 
                [globalEnv, func, fnArgs]() -> Value {
                    Interpreter threadInterp;
                    threadInterp.setGlobalEnv(globalEnv);
                    return threadInterp.callFunction(func, fnArgs, 0, "native");
                }).share();
                
            return Value::makeFuture(fut);
        }));

    auto awaitFn = [](Interpreter& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isFuture()) { interp.runtimeError("await() expects future", 0, ""); return Value(); }
        auto fut = args[0].asFuture();
        fut->wait();
        return fut->get();
    };
    interp.defineGlobal("await", Value::makeNativeFunction("await", 1, awaitFn));
    interp.defineGlobal("sync", Value::makeNativeFunction("sync", 1, awaitFn));


    // FileSys methods extraction
    interp.defineGlobal("fs_mkdir", Value::makeNativeFunction("fs_mkdir", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { return Value(std::filesystem::create_directories(args[0].toString())); }
            catch (...) { return Value(false); }
        }));

    interp.defineGlobal("fs_exists", Value::makeNativeFunction("fs_exists", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { return Value(std::filesystem::exists(args[0].toString())); }
            catch (...) { return Value(false); }
        }));

    interp.defineGlobal("fs_is_dir", Value::makeNativeFunction("fs_is_dir", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { return Value(std::filesystem::is_directory(args[0].toString())); }
            catch (...) { return Value(false); }
        }));

    interp.defineGlobal("fs_list_dir", Value::makeNativeFunction("fs_list_dir", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try {
                std::vector<Value> result;
                for (const auto& entry : std::filesystem::directory_iterator(args[0].toString())) {
                    result.push_back(Value(entry.path().filename().string()));
                }
                return Value::makeArray(result);
            } catch (...) { return Value::makeArray({}); }
        }));

    interp.defineGlobal("fs_copy", Value::makeNativeFunction("fs_copy", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { 
                std::filesystem::copy(args[0].toString(), args[1].toString(), std::filesystem::copy_options::overwrite_existing); 
                return Value(true);
            } catch (...) { return Value(false); }
        }));

    interp.defineGlobal("fs_move", Value::makeNativeFunction("fs_move", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { 
                std::filesystem::rename(args[0].toString(), args[1].toString()); 
                return Value(true);
            } catch (...) { return Value(false); }
        }));

    interp.defineGlobal("fs_remove", Value::makeNativeFunction("fs_remove", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { return Value((double)std::filesystem::remove_all(args[0].toString())); }
            catch (...) { return Value(0.0); }
        }));

    interp.defineGlobal("fs_size", Value::makeNativeFunction("fs_size", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { return Value((double)std::filesystem::file_size(args[0].toString())); }
            catch (...) { return Value(-1.0); }
        }));

    interp.defineGlobal("fs_modified_time", Value::makeNativeFunction("fs_modified_time", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { 
                auto ftime = std::filesystem::last_write_time(args[0].toString());
                auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(ftime - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
                long long ms = std::chrono::duration_cast<std::chrono::milliseconds>(sctp.time_since_epoch()).count();
                return Value((double)ms);
            } catch (...) { return Value(-1.0); }
        }));

    // OS Operations extraction
    interp.defineGlobal("os_get_env", Value::makeNativeFunction("os_get_env", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            const char* val = std::getenv(args[0].toString().c_str());
            if (val) return Value(std::string(val));
            return Value();
        }));

    interp.defineGlobal("os_set_env", Value::makeNativeFunction("os_set_env", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            _putenv_s(args[0].toString().c_str(), args[1].toString().c_str());
            return Value();
        }));

    interp.defineGlobal("os_exec", Value::makeNativeFunction("os_exec", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string cmd = args[0].toString();
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (!pipe) return Value("");
            char buffer[128];
            std::string result = "";
            while (!feof(pipe)) {
                if (fgets(buffer, 128, pipe) != nullptr)
                    result += buffer;
            }
            _pclose(pipe);
            return Value(result);
        }));

    interp.defineGlobal("os_system", Value::makeNativeFunction("os_system", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            int code = std::system(args[0].toString().c_str());
            return Value((double)code);
        }));

    interp.defineGlobal("os_cwd", Value::makeNativeFunction("os_cwd", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            try { return Value(std::filesystem::current_path().string()); }
            catch (...) { return Value(""); }
        }));

    interp.defineGlobal("os_chdir", Value::makeNativeFunction("os_chdir", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            try { std::filesystem::current_path(args[0].toString()); return Value(true); }
            catch (...) { return Value(false); }
        }));

    interp.defineGlobal("os_exit", Value::makeNativeFunction("os_exit", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::exit((int)args[0].asNumber());
            return Value();
        }));

    interp.defineGlobal("os_platform", Value::makeNativeFunction("os_platform", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            return Value("windows");
#elif __APPLE__
            return Value("darwin");
#else
            return Value("linux");
#endif
        }));

    interp.defineGlobal("os_uptime", Value::makeNativeFunction("os_uptime", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            return Value((double)(GetTickCount64() / 1000.0));
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_total_memory", Value::makeNativeFunction("os_total_memory", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            return Value((double)memInfo.ullTotalPhys);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_free_memory", Value::makeNativeFunction("os_free_memory", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            MEMORYSTATUSEX memInfo;
            memInfo.dwLength = sizeof(MEMORYSTATUSEX);
            GlobalMemoryStatusEx(&memInfo);
            return Value((double)memInfo.ullAvailPhys);
#else
            return Value(0.0);
#endif
        }));

    interp.defineGlobal("os_clipboard_read", Value::makeNativeFunction("os_clipboard_read", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            if (!OpenClipboard(nullptr)) return Value("");
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (!hData) { CloseClipboard(); return Value(""); }
            char* pszText = static_cast<char*>(GlobalLock(hData));
            if (!pszText) { CloseClipboard(); return Value(""); }
            std::string text(pszText);
            GlobalUnlock(hData);
            CloseClipboard();
            return Value(text);
#else
            return Value("");
#endif
        }));

    interp.defineGlobal("os_clipboard_write", Value::makeNativeFunction("os_clipboard_write", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            std::string text = args[0].toString();
            if (!OpenClipboard(nullptr)) return Value(false);
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (!hMem) { CloseClipboard(); return Value(false); }
            memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
            CloseClipboard();
            return Value(true);
#else
            return Value(false);
#endif
        }));

    interp.defineGlobal("os_shell_open", Value::makeNativeFunction("os_shell_open", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            HINSTANCE res = ShellExecuteA(NULL, "open", args[0].toString().c_str(), NULL, NULL, SW_SHOWNORMAL);
            return Value((double)(reinterpret_cast<INT_PTR>(res)));
#else
            return Value(0.0);
#endif
        }));
        
    interp.defineGlobal("clear", Value::makeNativeFunction("clear", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            system("cls");
            return Value();
        }));

    interp.defineGlobal("color", Value::makeNativeFunction("color", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) { interp.runtimeError("color() expects a number code (0-15)", 0, ""); return Value(); }
            int code = (int)args[0].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, (WORD)code);
#endif
            return Value();
        }));

    interp.defineGlobal("reset", Value::makeNativeFunction("reset", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 7); 
#endif
            return Value();
        }));

    interp.defineGlobal("gotoxy", Value::makeNativeFunction("gotoxy", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) 
                { interp.runtimeError("gotoxy() expects two numbers (x, y)", 0, ""); return Value(); }
            int x = (int)args[0].asNumber();
            int y = (int)args[1].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(hConsole, pos);
#endif
            return Value();
        }));

    interp.defineGlobal("getch", Value::makeNativeFunction("getch", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            int c = _getch();
            return Value(std::string(1, (char)c));
#else
            return Value("");
#endif
        }));
}
