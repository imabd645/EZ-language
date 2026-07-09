#include "runtime/objects/EZObjects.h"
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

void registerCoreBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("panic", Value::makeNativeFunction("panic", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            interp.runtimeError(args[0].toString(), 0, "");
            return Value();
        }));

}
