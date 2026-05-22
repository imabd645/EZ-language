#include "../Builtins.h"
#include "../RuntimeContext.h"
#include "../GC.h"
#include "../Environment.h"
#include <thread>
#include <chrono>
#include <iostream>

void registerConcurrencyBuiltins(RuntimeContext& interp) {
    // mutex()
    interp.defineGlobal("mutex", Value::makeNativeFunction("mutex", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value(makeGCMutex());
        }));

    // lock(mutex, lambda)
    interp.defineGlobal("lock", Value::makeNativeFunction("lock", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isMutex()) {
                interp.runtimeError("lock() expects mutex as first argument", 0, "");
                return Value();
            }
            if (!args[1].isCallable()) {
                interp.runtimeError("lock() expects lambda/function as second argument", 0, "");
                return Value();
            }
            
            auto mtx = args[0].asMutexPtr();
            
            // RAII lock — automatically unlocks on scope exit, even if an exception is thrown
            std::lock_guard<std::recursive_mutex> guard(mtx->mtx);
            return interp.callFunction(args[1], {}, 0, "");
        }));

    // wait(ms)
    interp.defineGlobal("wait", Value::makeNativeFunction("wait", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
    
    // atomic_inc(name) — atomically increment a GLOBAL variable by 1
    interp.defineGlobal("atomic_inc", Value::makeNativeFunction("atomic_inc", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_inc() expects variable name", 0, ""); return Value(); }
            std::string name = args[0].asString();
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long current_val = 0;
            if (val.isInteger()) current_val = val.asInteger();
            else if (val.isFloat()) current_val = static_cast<long long>(val.asFloat());
            
            long long new_val = current_val + 1;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));

    // atomic_dec(name) — atomically decrement a GLOBAL variable by 1
    interp.defineGlobal("atomic_dec", Value::makeNativeFunction("atomic_dec", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_dec() expects variable name", 0, ""); return Value(); }
            std::string name = args[0].asString();
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long current_val = 0;
            if (val.isInteger()) current_val = val.asInteger();
            else if (val.isFloat()) current_val = static_cast<long long>(val.asFloat());
            
            long long new_val = current_val - 1;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));

    // atomic_add(name, amount) — atomically add amount to a GLOBAL variable
    interp.defineGlobal("atomic_add", Value::makeNativeFunction("atomic_add", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("atomic_add() expects variable name", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("atomic_add() expects number amount", 0, ""); return Value(); }
            std::string name = args[0].asString();
            long long amount = args[1].isInteger() ? args[1].asInteger() : static_cast<long long>(args[1].asFloat());
            std::lock_guard<std::mutex> lock(atomicMutex);
            auto env = interp.getGlobalEnv();
            Value val = env->get(name, 0);
            long long current_val = 0;
            if (val.isInteger()) current_val = val.asInteger();
            else if (val.isFloat()) current_val = static_cast<long long>(val.asFloat());
            
            long long new_val = current_val + amount;
            env->assign(name, Value(new_val));
            return Value(new_val);
        }));
}
