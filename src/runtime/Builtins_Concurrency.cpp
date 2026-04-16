#include "../Builtins.h"
#include "../Interpreter.h"
#include "../GC.h"
#include <thread>
#include <chrono>

void registerConcurrencyBuiltins(Interpreter& interp) {
    // mutex()
    interp.defineGlobal("mutex", Value::makeNativeFunction("mutex", 0,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return Value(makeGCMutex());
        }));

    // lock(mutex, lambda)
    interp.defineGlobal("lock", Value::makeNativeFunction("lock", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isMutex()) {
                interp.runtimeError("lock() expects mutex as first argument", 0, "");
                return Value();
            }
            if (!args[1].isCallable()) {
                interp.runtimeError("lock() expects lambda/function as second argument", 0, "");
                return Value();
            }
            
            auto mtx = args[0].asMutexPtr();
            
            // RAII Lock
            mtx->lock();
            try {
                Value result = interp.callFunction(args[1], {}, 0, "");
                mtx->unlock();
                return result;
            } catch (...) {
                mtx->unlock();
                throw; // Re-throw to let interpreter handle error
            }
        }));

    // wait(ms)
    interp.defineGlobal("wait", Value::makeNativeFunction("wait", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                interp.runtimeError("wait() expects milliseconds (number)", 0, "");
                return Value();
            }
            int ms = static_cast<int>(args[0].asInteger());
            if (ms > 0) {
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            return Value();
        }));
}
