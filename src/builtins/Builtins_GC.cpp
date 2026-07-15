#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "runtime/Environment.h"
#include "vm/BytecodeVM.h"
#include "runtime/EZFuture.h"
#include "eventloop/EventLoop.h"
#include <iostream>
#include <chrono>
#include <thread>
#include <future>
#include <vector>
#include <string>

void registerGCBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("gc_disable", Value::makeNativeFunction("gc_disable", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            CycleCollector::instance().disable();
            return Value(true);
        }));

    interp.defineGlobal("gc_enable", Value::makeNativeFunction("gc_enable", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            CycleCollector::instance().enable();
            return Value(true);
        }));

    interp.defineGlobal("gc_collect", Value::makeNativeFunction("gc_collect", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            CycleCollector::instance().collect();
            return Value(true);
        }));

    interp.defineGlobal("gc_set_thresholds", Value::makeNativeFunction("gc_set_thresholds", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isNumber() && args[1].isNumber()) {
                CycleCollector::instance().setThresholds(
                    (size_t)args[0].asNumber(),
                    (size_t)args[1].asNumber()
                );
                return Value(true);
            }
            return Value(false);
        }));

    interp.defineGlobal("exit", Value::makeNativeFunction("exit", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            int code = 0;
            if (!args.empty() && args[0].isNumber()) code = (int)args[0].asNumber();
            std::exit(code);
            return Value();
        }));

    auto exceptionClass = std::make_shared<EZClass>("Exception");
    CycleCollector::instance().track(exceptionClass, ValueType::CLASS);
    exceptionClass->methods["init"] = Value::makeNativeFunction("init", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && args[0].isInstance()) {
                auto inst = args[0].asInstance();
                std::string message = args.size() > 1 ? args[1].toString() : "Unknown Error";
                inst->setProperty("message", Value(message));
            }
            return Value();
        });
    interp.defineGlobal("Exception", Value(exceptionClass));

    auto makeErrorClass = [&](const std::string& name, std::shared_ptr<EZClass> parent) {
        auto cls = std::make_shared<EZClass>(name);
        CycleCollector::instance().track(cls, ValueType::CLASS);
        cls->parent = parent;
        interp.defineGlobal(name, Value(cls));
        return cls;
    };

    makeErrorClass("FileNotFoundError", exceptionClass);
    makeErrorClass("NetworkError", exceptionClass);
    makeErrorClass("TypeError", exceptionClass);
    makeErrorClass("ValueError", exceptionClass);
    makeErrorClass("IndexError", exceptionClass);
    makeErrorClass("KeyError", exceptionClass);
    makeErrorClass("PermissionError", exceptionClass);

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


            // Close upvalues in any closure/bound-method so the worker thread doesn't hold
            // dangling pointers into the parent VM's stack.
            // Arrays, Dicts, and Instances are shared_ptr based  we keep them as-is so
            // worker threads share the same queue/mutex objects.
            auto threadUpvalues = std::make_shared<std::vector<std::unique_ptr<UpvalueObj>>>();
            std::unordered_map<void*, Value> seen;

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
                // Arrays, Dicts, Instances, primitives  share directly
                return v;
            };

            Value closedFunc = closeUpvals(func);
            std::vector<Value> closedArgs;
            for (auto& a : fnArgs) closedArgs.push_back(closeUpvals(a));

            auto ezFut = std::make_shared<EZFuture>();

            EventLoop::instance().retain();
            std::thread([ezFut, globalEnv, closedFunc, closedArgs, threadUpvalues]() {
                try {
                    auto threadVM = std::make_shared<BytecodeVM>(globalEnv);
                    threadVM->traceExecution = false;

                    threadVM->taskFuture = ezFut;
                    for (auto& uv : *threadUpvalues) threadVM->adoptUpvalue(std::move(uv));
                    Value result = threadVM->callFunction(closedFunc, closedArgs, 0, "native");
                    
                    if (!threadVM->isYielded) {
                        ezFut->set(result);
                    }
                } catch(std::exception& e) {
                    std::cerr << "[spawn-thread] uncaught: " << e.what() << std::endl;
                    ezFut->set(Value());
                }
                EventLoop::instance().release();
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

    interp.defineGlobal("cancel", Value::makeNativeFunction("cancel", 1, 
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isFuture()) { interp.runtimeError("cancel() expects future", 0, ""); return Value(); }
            args[0].asFuture()->cancel();
            return Value();
        }));

    interp.defineGlobal("awaitAll", Value::makeNativeFunction("awaitAll", 1, 
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("awaitAll() expects array of futures", 0, ""); return Value(); }
            auto& arr = args[0].asArray();
            std::vector<Value> results;
            for (auto& v : arr.getElementsCopy()) {
                if (!v.isFuture()) { interp.runtimeError("awaitAll() array must contain only futures", 0, ""); return Value(); }
                auto fut = v.asFuture();
                fut->wait();
                results.push_back(fut->get());
            }
            return Value::makeArray(results);
        }));

    interp.defineGlobal("awaitAny", Value::makeNativeFunction("awaitAny", 1, 
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("awaitAny() expects array of futures", 0, ""); return Value(); }
            auto& arr = args[0].asArray();
            if (arr.empty()) { interp.runtimeError("awaitAny() cannot accept empty array", 0, ""); return Value(); }
            
            // For awaitAny, we check if any are ready.
            // If none are ready, we could wait on multiple events via WaitForMultipleObjects,
            // but since futures hold handles, we can collect them.
            std::vector<HANDLE> handles;
            for (auto& v : arr.getElementsCopy()) {
                if (!v.isFuture()) { interp.runtimeError("awaitAny() array must contain only futures", 0, ""); return Value(); }
                handles.push_back(v.asFuture()->hEvent);
            }
            
            DWORD result = WaitForMultipleObjects(handles.size(), handles.data(), false, INFINITE);
            if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + handles.size()) {
                size_t index = result - WAIT_OBJECT_0;
                return arr[index].asFuture()->get();
            }
            
            interp.runtimeError("awaitAny() failed to wait", 0, "");
            return Value();
        }));



    // Legacy std::filesystem C++ built-ins fully extracted into pure native EZ lib/fs.ez via FFI.

    // Legacy OS Operations extracted to lib/os.ez via FFI
        
}
