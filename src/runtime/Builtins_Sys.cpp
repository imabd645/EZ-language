#include "../Builtins.h"
#include "../RuntimeContext.h"
#include "../BytecodeVM.h"
#include "../GUIBuiltins.h"

#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <filesystem>
#include <vector>
#include <string>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define NOCOMM
#include <windows.h>
#include <setjmp.h>
#include <conio.h>
#include <shellapi.h>

// Include EZFuture (Windows-native future, must be after windows.h)

#include "../EZFuture.h"

#ifdef _WIN32

#ifdef _MSC_VER
static intptr_t do_ffi_call(void* funcPtr, intptr_t* cArgs, size_t argc, bool isFloat, double& f_ret, bool& crashed) {
    intptr_t ret = 0;
    crashed = false;
    __try {
        if (isFloat) {
            using fFunc0 = double(*)(); using fFunc1 = double(*)(intptr_t); using fFunc2 = double(*)(intptr_t, intptr_t);
            using fFunc3 = double(*)(intptr_t, intptr_t, intptr_t); using fFunc4 = double(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) f_ret = ((fFunc0)funcPtr)();
            else if (argc == 1) f_ret = ((fFunc1)funcPtr)(cArgs[0]);
            else if (argc == 2) f_ret = ((fFunc2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) f_ret = ((fFunc3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc >= 4) f_ret = ((fFunc4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
        } else {
            using Func0 = intptr_t(*)(); using Func1 = intptr_t(*)(intptr_t); using Func2 = intptr_t(*)(intptr_t, intptr_t);
            using Func3 = intptr_t(*)(intptr_t, intptr_t, intptr_t); using Func4 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            using Func5 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func6 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func7 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func8 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func9 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func10 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func11 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func12 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) ret = ((Func0)funcPtr)();
            else if (argc == 1) ret = ((Func1)funcPtr)(cArgs[0]);
            else if (argc == 2) ret = ((Func2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) ret = ((Func3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc == 4) ret = ((Func4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
            else if (argc == 5) ret = ((Func5)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4]);
            else if (argc == 6) ret = ((Func6)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5]);
            else if (argc == 7) ret = ((Func7)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6]);
            else if (argc == 8) ret = ((Func8)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7]);
            else if (argc == 9) ret = ((Func9)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8]);
            else if (argc == 10) ret = ((Func10)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9]);
            else if (argc == 11) ret = ((Func11)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10]);
            else if (argc >= 12) ret = ((Func12)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10], cArgs[11]);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        crashed = true;
    }
    return ret;
}
#else
static jmp_buf os_call_jmp_env;
static LONG CALLBACK FfiVectoredHandler(PEXCEPTION_POINTERS ExceptionInfo) {
    DWORD code = ExceptionInfo->ExceptionRecord->ExceptionCode;
    if (code == EXCEPTION_ACCESS_VIOLATION ||
        code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_PRIV_INSTRUCTION) {
        longjmp(os_call_jmp_env, 1);
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static intptr_t do_ffi_call(void* funcPtr, intptr_t* cArgs, size_t argc, bool isFloat, double& f_ret, bool& crashed) {
    intptr_t ret = 0;
    crashed = false;
    PVOID vehHandler = AddVectoredExceptionHandler(1, FfiVectoredHandler);
    if (setjmp(os_call_jmp_env) == 0) {
        if (isFloat) {
            using fFunc0 = double(*)(); using fFunc1 = double(*)(intptr_t); using fFunc2 = double(*)(intptr_t, intptr_t);
            using fFunc3 = double(*)(intptr_t, intptr_t, intptr_t); using fFunc4 = double(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) f_ret = ((fFunc0)funcPtr)();
            else if (argc == 1) f_ret = ((fFunc1)funcPtr)(cArgs[0]);
            else if (argc == 2) f_ret = ((fFunc2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) f_ret = ((fFunc3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc >= 4) f_ret = ((fFunc4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
        } else {
            using Func0 = intptr_t(*)(); using Func1 = intptr_t(*)(intptr_t); using Func2 = intptr_t(*)(intptr_t, intptr_t);
            using Func3 = intptr_t(*)(intptr_t, intptr_t, intptr_t); using Func4 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t);
            using Func5 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func6 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func7 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func8 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func9 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func10 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            using Func11 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t); using Func12 = intptr_t(*)(intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t, intptr_t);
            if (argc == 0) ret = ((Func0)funcPtr)();
            else if (argc == 1) ret = ((Func1)funcPtr)(cArgs[0]);
            else if (argc == 2) ret = ((Func2)funcPtr)(cArgs[0], cArgs[1]);
            else if (argc == 3) ret = ((Func3)funcPtr)(cArgs[0], cArgs[1], cArgs[2]);
            else if (argc == 4) ret = ((Func4)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3]);
            else if (argc == 5) ret = ((Func5)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4]);
            else if (argc == 6) ret = ((Func6)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5]);
            else if (argc == 7) ret = ((Func7)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6]);
            else if (argc == 8) ret = ((Func8)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7]);
            else if (argc == 9) ret = ((Func9)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8]);
            else if (argc == 10) ret = ((Func10)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9]);
            else if (argc == 11) ret = ((Func11)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10]);
            else if (argc >= 12) ret = ((Func12)funcPtr)(cArgs[0], cArgs[1], cArgs[2], cArgs[3], cArgs[4], cArgs[5], cArgs[6], cArgs[7], cArgs[8], cArgs[9], cArgs[10], cArgs[11]);
        }
    } else {
        crashed = true;
    }
    RemoveVectoredExceptionHandler(vehHandler);
    return ret;
}
#endif
#endif

static LRESULT CALLBACK EZ_ProxyWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    // Only redirect SENT messages (WM_COMMAND, WM_NOTIFY)
    // Redirect with 0x8000 offset to avoid infinite loop when DispatchMessage calls this proxy
    if (msg == WM_COMMAND || msg == WM_NOTIFY) {
        PostMessage(hwnd, msg + 0x8000, wParam, lParam);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}
#endif

void registerSysBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("panic", Value::makeNativeFunction("panic", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            interp.runtimeError(args[0].toString(), 0, "");
            return Value();
        }));

    interp.defineGlobal("exit", Value::makeNativeFunction("exit", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int code = 0;
            if (!args.empty() && args[0].isNumber()) code = (int)args[0].asNumber();
            std::exit(code);
            return Value();
        }));

    interp.defineGlobal("Exception", Value::makeNativeFunction("Exception", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string message = args.size() > 0 ? args[0].toString() : "Unknown Error";
            double code = args.size() > 1 && args[1].isNumber() ? args[1].asNumber() : 0.0;
            
            auto dict = std::make_shared<EZDictionary>();
            dict->map["message"] = Value(message);
            dict->map["code"] = Value(code);
            dict->map["stackTrace"] = Value("");
            
            return Value(dict);
        }));

    interp.defineGlobal("clock", Value::makeNativeFunction("clock", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            return Value((double)ms);
        }));

    interp.defineGlobal("stop", Value::makeNativeFunction("stop", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("stop() expects number", 0, ""); return Value(); }
            std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asNumber()));
            return Value();
        }));

    interp.defineGlobal("spawn", Value::makeNativeFunction("spawn", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isCallable()) { interp.runtimeError("spawn() expects function", 0, ""); return Value(); }
            Value func = args[0];
            std::vector<Value> fnArgs(args.begin() + 1, args.end());
            auto globalEnv = interp.getGlobalEnv();
            BytecodeVM* parentVM = dynamic_cast<BytecodeVM*>(&interp);
            auto tState = parentVM ? parentVM->exportThreadState() : BytecodeVM::ThreadState();

            // Close upvalues in any closure/bound-method so the worker thread doesn't hold
            // dangling pointers into the parent VM's stack.
            // Arrays, Dicts, and Instances are shared_ptr based — we keep them as-is so
            // worker threads share the same queue/mutex objects.
            auto threadUpvalues = std::make_shared<std::vector<std::unique_ptr<UpvalueObj>>>();
            std::unordered_map<GCObject*, Value> seen;

            std::function<Value(const Value&)> closeUpvals = [&](const Value& v) -> Value {
                if (v.isClosure()) {
                    auto oldCl = v.asClosure();
                    if (seen.count(oldCl.get())) return seen[oldCl.get()];

                    auto newCl = std::make_shared<EZClosure>(oldCl->function);
                    seen[oldCl.get()] = Value(newCl);

                    for (auto* uv : oldCl->upvalues) {
                        if (!uv) { newCl->upvalues.push_back(nullptr); continue; }
                        auto newUv = std::make_unique<UpvalueObj>();
                        Value* loc = uv->location.load();
                        // Snapshot the current value (which may be on parent's stack)
                        Value snap = (loc != nullptr) ? *loc : Value();
                        // Recursively close nested closures captured in this upvalue
                        newUv->closed = closeUpvals(snap);
                        newUv->location.store(&newUv->closed);
                        newUv->next = nullptr;
                        newCl->upvalues.push_back(newUv.get());
                        threadUpvalues->push_back(std::move(newUv));
                    }
                    return Value(newCl);
                } else if (v.isBoundMethod()) {
                    auto oldBm = v.asBoundMethod();
                    if (seen.count(oldBm.get())) return seen[oldBm.get()];
                    auto newBm = std::make_shared<EZBoundMethod>(
                        closeUpvals(oldBm->receiver),
                        closeUpvals(oldBm->method)
                    );
                    seen[oldBm.get()] = Value(newBm);
                    return Value(newBm);
                }
                // Arrays, Dicts, Instances, primitives — share directly
                return v;
            };

            Value closedFunc = closeUpvals(func);
            std::vector<Value> closedArgs;
            for (auto& a : fnArgs) closedArgs.push_back(closeUpvals(a));

            auto ezFut = std::make_shared<EZFuture>();

            std::thread([ezFut, globalEnv, closedFunc, closedArgs, tState, threadUpvalues]() {
                try {
                    BytecodeVM threadVM(globalEnv);
                    threadVM.traceExecution = false;
                    threadVM.importThreadState(tState);
                    for (auto& uv : *threadUpvalues) threadVM.adoptUpvalue(std::move(uv));
                    Value result = threadVM.callFunction(closedFunc, closedArgs, 0, "native");
                    ezFut->set(result);
                } catch(std::exception& e) {
                    std::cerr << "[spawn-thread] uncaught: " << e.what() << std::endl;
                    ezFut->set(Value());
                }
            }).detach();

            return Value::makeFuture(ezFut);
        }));

    auto awaitFn = [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isFuture()) { interp.runtimeError("await() expects future", 0, ""); return Value(); }
        auto fut = args[0].asFuture();
        fut->wait();
        return fut->get();
    };
    interp.defineGlobal("await", Value::makeNativeFunction("await", 1, awaitFn));
    interp.defineGlobal("sync", Value::makeNativeFunction("sync", 1, awaitFn));



    // Legacy std::filesystem C++ built-ins fully extracted into pure native EZ lib/fs.ez via FFI.

    // Legacy OS Operations extracted to lib/os.ez via FFI
        
    interp.defineGlobal("clear", Value::makeNativeFunction("clear", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            system("cls");
            return Value();
        }));

    interp.defineGlobal("color", Value::makeNativeFunction("color", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) { interp.runtimeError("color() expects a number code (0-15)", 0, ""); return Value(); }
            int code = (int)args[0].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, (WORD)code);
#endif
            return Value();
        }));

    interp.defineGlobal("reset", Value::makeNativeFunction("reset", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 7); 
#endif
            return Value();
        }));

    interp.defineGlobal("gotoxy", Value::makeNativeFunction("gotoxy", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            int c = _getch();
            return Value(std::string(1, (char)c));
#else
            return Value("");
#endif
        }));

    interp.defineGlobal("os_alloc", Value::makeNativeFunction("os_alloc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            size_t size = (size_t)args[0].asNumber();
            void* ptr = calloc(1, size);
            return Value((long long)(reinterpret_cast<uintptr_t>(ptr)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_free", Value::makeNativeFunction("os_free", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value();
            void* ptr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (ptr) free(ptr);
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_uint64", Value::makeNativeFunction("os_read_uint64", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint64_t val = *(uint64_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_uint32", Value::makeNativeFunction("os_read_uint32", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint32_t val = *(uint32_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint32", Value::makeNativeFunction("os_write_uint32", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint32_t*)(base + offset) = (uint32_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));



    interp.defineGlobal("os_get_proxy_wndproc", Value::makeNativeFunction("os_get_proxy_wndproc", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            return Value((long long)(uintptr_t)EZ_ProxyWndProc);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_read_uint16", Value::makeNativeFunction("os_read_uint16", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            uint16_t val = *(uint16_t*)(base + offset);
            return Value((long long)val);
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_uint16", Value::makeNativeFunction("os_write_uint16", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint16_t*)(base + offset) = (uint16_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_write_uint64", Value::makeNativeFunction("os_write_uint64", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(uint64_t*)(base + offset) = (uint64_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_byte", Value::makeNativeFunction("os_read_byte", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber()) return Value(0LL);
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            return Value((long long)*(base + offset));
#else
            return Value(0LL);
#endif
        }));

    interp.defineGlobal("os_write_byte", Value::makeNativeFunction("os_write_byte", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber()) return Value();
            uint8_t* base = reinterpret_cast<uint8_t*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            *(base + offset) = (uint8_t)args[2].asNumber();
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_read_string_ptr", Value::makeNativeFunction("os_read_string_ptr", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber()) return Value("");
            const char* str = reinterpret_cast<const char*>((uintptr_t)args[0].asNumber());
            if (str) return Value(std::string(str));
            return Value("");
#else
            return Value("");
#endif
        }));

    interp.defineGlobal("os_write_string", Value::makeNativeFunction("os_write_string", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isString()) return Value();
            char* base = reinterpret_cast<char*>((uintptr_t)args[0].asNumber());
            size_t offset = (size_t)args[1].asNumber();
            std::string text = args[2].asString();
            memcpy(base + offset, text.c_str(), text.length() + 1);
            return Value();
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_load_lib", Value::makeNativeFunction("os_load_lib", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isString()) return Value();
            HMODULE handle = LoadLibraryA(args[0].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(handle)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_get_func", Value::makeNativeFunction("os_get_func", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (!args[0].isNumber() || !args[1].isString()) return Value();
            HMODULE handle = reinterpret_cast<HMODULE>((uintptr_t)args[0].asNumber());
            FARPROC proc = GetProcAddress(handle, args[1].asString().c_str());
            return Value((long long)(reinterpret_cast<uintptr_t>(proc)));
#else
            return Value();
#endif
        }));

    interp.defineGlobal("os_call", Value::makeNativeFunction("os_call", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
#ifdef _WIN32
            if (args.size() < 2 || !args[0].isNumber() || !args[1].isString()) return Value();
            void* funcPtr = reinterpret_cast<void*>((uintptr_t)args[0].asNumber());
            if (!funcPtr) {
                interp.runtimeError("os_call: Null function pointer or function not found.", 0, "");
                return Value();
            }
            
            intptr_t cArgs[12] = {0};
            for (size_t i = 2; i < args.size() && i - 2 < 12; i++) {
                if (args[i].isNumber()) cArgs[i - 2] = static_cast<intptr_t>(args[i].asNumber());
                else if (args[i].isString()) cArgs[i - 2] = reinterpret_cast<intptr_t>(args[i].asString().c_str());
                else if (args[i].isBuffer()) cArgs[i - 2] = reinterpret_cast<intptr_t>(args[i].asBuffer().data());
                else if (args[i].isBool()) cArgs[i - 2] = args[i].asBool() ? 1 : 0;
            }
            
            size_t argc = args.size() - 2;
            std::string retType = args[1].asString();

            bool crashed = false;
            double f_ret = 0.0;
            intptr_t ret = do_ffi_call(funcPtr, cArgs, argc, (retType == "float"), f_ret, crashed);

            if (crashed) {
                interp.runtimeError("os_call: Access violation or fatal memory fault inside external DLL.", 0, "");
                return Value();
            }
            
            if (retType == "float") return Value(f_ret);
            
            if (retType == "int" || retType == "ptr") return Value((long long)ret);
            if (retType == "string") {
                const char* str = reinterpret_cast<const char*>(ret);
                if (str) return Value(std::string(str));
                return Value("");
            }
            return Value();
#else
            return Value();
#endif
        }));
}