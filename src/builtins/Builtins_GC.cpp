#include "runtime/RuntimeContext.h"
#include "vm/BytecodeVM.h"
#include "../EZFuture.h"
#include "builtins/Builtins.h"
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
    exceptionClass->methods["init"] = Value::makeNativeFunction("init", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() > 0 && args[0].isInstance()) {
                auto inst = args[0].asInstance();
                std::string message = args.size() > 1 ? args[1].toString() : "Unknown Error";
                inst->properties["message"] = Value(message);
            }
            return Value();
        });
    interp.defineGlobal("Exception", Value(exceptionClass));

    auto makeErrorClass = [&](const std::string& name, std::shared_ptr<EZClass> parent) {
        auto cls = std::make_shared<EZClass>(name);
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
            BytecodeVM* parentVM = dynamic_cast<BytecodeVM*>(&interp);
            auto tState = parentVM ? parentVM->exportThreadState() : BytecodeVM::ThreadState();

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

            std::vector<std::string> slotNames;
            std::vector<Value> slotValues;
            if (parentVM) {
                slotNames = parentVM->getGlobalSlotNames();
                slotValues = parentVM->getGlobalSlots();
            }

            std::thread([ezFut, globalEnv, slotNames, slotValues, closedFunc, closedArgs, tState, threadUpvalues]() {
                try {
                    BytecodeVM threadVM(globalEnv);
                    threadVM.setGlobalSlots(slotNames, slotValues);
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
        
}
