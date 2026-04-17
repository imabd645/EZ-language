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

    static std::mutex atomicMutex;
    
    // atomic_inc(name)
    interp.defineGlobal("atomic_inc", Value::makeNativeFunction("atomic_inc", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_inc() expects variable name", 0, ""); return Value(); }
            std::string name = args[0].asString();
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long new_val = (val.isInteger() ? val.asInteger() : (long long)val.asNumber()) + 1;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));

    // atomic_dec(name)
    interp.defineGlobal("atomic_dec", Value::makeNativeFunction("atomic_dec", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_dec() expects variable name", 0, ""); return Value(); }
            std::string name = args[0].asString();
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long new_val = (val.isInteger() ? val.asInteger() : (long long)val.asNumber()) - 1;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));

    // atomic_add(name, amount)
    interp.defineGlobal("atomic_add", Value::makeNativeFunction("atomic_add", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_add() expects variable name", 0, ""); return Value(); }
            if (!args[1].isNumber() && !args[1].isInteger()) { interp.runtimeError("atomic_add() expects number amount", 0, ""); return Value(); }
            std::string name = args[0].asString();
            long long amount = args[1].isInteger() ? args[1].asInteger() : (long long)args[1].asNumber();
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long new_val = (val.isInteger() ? val.asInteger() : (long long)val.asNumber()) + amount;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));
}
