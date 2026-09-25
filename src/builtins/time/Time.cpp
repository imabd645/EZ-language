#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <string>
#include <vector>
#include <chrono>
#include <thread>

void registerTimeBuiltins(RuntimeContext& interp) {
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
            GCSafeRegion safe;   // sleeping: see wait()
            std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asNumber()));
            return Value();
        }));
}
