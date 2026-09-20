#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <string>
#include <vector>
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

void registerProcessBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("__process_pid", Value::makeNativeFunction("__process_pid", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
#ifdef _WIN32
            return Value((long long)_getpid());
#else
            return Value((long long)getpid());
#endif
        }));

    interp.defineGlobal("__process_cwd", Value::makeNativeFunction("__process_cwd", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            try {
                return Value(std::filesystem::current_path().string());
            } catch (...) {
                return Value("");
            }
        }));

    interp.defineGlobal("exit", Value::makeNativeFunction("exit", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int code = 0;
            if (!args.empty() && args[0].isNumber()) code = (int)args[0].asNumber();
            std::exit(code);
            return Value();
        }));
}
