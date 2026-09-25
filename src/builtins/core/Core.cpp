#include "runtime/objects/EZObjects.h"
#include "runtime/RuntimeContext.h"
#include "runtime/SecurityPolicy.h"
#include "vm/BytecodeVM.h"
#include "runtime/EZFuture.h"
#include "builtins/Builtins.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <string>

// Filled in by the CLI before the program runs; see Builtins.h.
std::vector<std::string> g_scriptArgs;
std::string g_scriptName;

void registerCoreBuiltins(RuntimeContext& interp) {
    // argv — the arguments passed to the script, as a list of strings.
    //
    // Interpreter flags are NOT included: `ez app.ez --trace -- --trace` runs
    // the interpreter with tracing and hands the script one argument, the
    // literal "--trace". Without that separation a program could never accept
    // an option that happens to share a name with an interpreter one.
    //
    // Always a list, never nil, so `len(argv)` is safe with no arguments.
    {
        std::vector<Value> items;
        items.reserve(g_scriptArgs.size());
        for (const auto& arg : g_scriptArgs) items.push_back(Value(arg));
        interp.defineGlobal("argv", Value::makeArray(items));
    }

    // scriptName — the path the script was invoked as, for usage messages.
    interp.defineGlobal("scriptName", Value(g_scriptName));

    interp.defineGlobal("panic", Value::makeNativeFunction("panic", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            interp.runtimeError(args[0].toString(), 0, "");
            return Value();
        }));

    interp.defineGlobal("eval", Value::makeNativeFunction("eval", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                interp.runtimeError("eval() expects a string argument", 0, "");
                return Value();
            }
            return interp.eval(args[0].asString(), "<eval>");
        }));

    auto osFn = Value::makeNativeFunction("os", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
#if defined(_WIN32)
            return Value("windows");
#elif defined(__APPLE__)
            return Value("macos");
#elif defined(__linux__)
            return Value("linux");
#else
            return Value("posix");
#endif
        });
    interp.defineGlobal("os", osFn);
    interp.defineGlobal("get_os", osFn);

    interp.defineGlobal("system", Value::makeNativeFunction("system", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.runtimeError("system() expects a command string", 0, "");
                return Value();
            }
            std::string cmd = args[0].asString();
            if (!SecurityPolicy::checkProcess(interp, cmd)) {
                return Value();
            }
            int res = std::system(cmd.c_str());
            return Value(static_cast<long long>(res));
        }));
}
