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
                GarbageCollector::instance().leaveVMExecution();
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                GarbageCollector::instance().enterVMExecution();
            }
            return Value();
        }));

    // class Atomic
    auto atomicClass = std::make_shared<EZClass>("Atomic");
    
    // Atomic.init(initial)
    atomicClass->methods["init"] = Value::makeNativeFunction("init", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            // args[0] is the instance, args[1] is the initial value
            auto instance = args[0].asInstance();
            long long initial = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            instance->setProperty("_atomic", Value::makeAtomic(initial));
            return args[0];
        });
        
    atomicClass->methods["get"] = Value::makeNativeFunction("get", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            return Value(atomic->val.load());
        });
        
    atomicClass->methods["set"] = Value::makeNativeFunction("set", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            atomic->val.store(v);
            return Value(v);
        });
        
    atomicClass->methods["add"] = Value::makeNativeFunction("add", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            return Value(atomic->val.fetch_add(v) + v);
        });
        
    atomicClass->methods["sub"] = Value::makeNativeFunction("sub", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            return Value(atomic->val.fetch_sub(v) - v);
        });
        
    interp.defineGlobal("Atomic", Value(atomicClass));
}
