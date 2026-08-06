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
#include <memory>
#include <mutex>
#include <condition_variable>

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

    // Number of objects currently tracked by the cycle collector. Useful for
    // leak assertions in tests.
    interp.defineGlobal("gc_tracked", Value::makeNativeFunction("gc_tracked", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)CycleCollector::instance().trackedCount());
        }));

    // Cumulative count of reference cycles reclaimed since startup. Compare
    // before/after a gc_collect() to observe how much was freed.
    interp.defineGlobal("gc_cycles_collected", Value::makeNativeFunction("gc_cycles_collected", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)CycleCollector::instance().cyclesCollected());
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
            // dangling pointers into the parent VM's stack. The new upvalues are
            // owned (shared_ptr) by the copied closures themselves, so they need no
            // separate lifetime management on the worker VM.
            // Arrays, Dicts, and Instances are shared_ptr based  we keep them as-is so
            // worker threads share the same queue/mutex objects.
            std::unordered_map<void*, Value> seen;

            std::function<Value(const Value&)> closeUpvals = [&](const Value& v) -> Value {
                if (v.isClosure()) {
                    auto oldCl = v.asClosure();
                    if (seen.count(oldCl.get())) return seen[oldCl.get()];

                    auto newCl = std::make_shared<EZClosure>(oldCl->function);
                    seen[oldCl.get()] = Value(newCl);

                    for (auto& uv : oldCl->upvalues) {
                        if (!uv) { newCl->upvalues.push_back(nullptr); continue; }
                        auto newUv = std::make_shared<UpvalueObj>();
                        Value* loc = uv->location.load();
                        // Snapshot the current value (which may be on parent's stack)
                        Value snap = (loc != nullptr) ? *loc : Value();
                        // Recursively close nested closures captured in this upvalue
                        newUv->closed = closeUpvals(snap);
                        newUv->location.store(&newUv->closed);
                        newUv->next = nullptr;
                        newCl->upvalues.push_back(newUv);   // closure owns the upvalue
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
            std::thread([ezFut, globalEnv, closedFunc, closedArgs]() {
                // Register as a concurrent mutator for the whole lifetime of this
                // worker so the cycle collector defers collection while we run
                // (it can't safely collect the shared object graph we mutate).
                // RAII ensures we unregister on every exit path.
                // Signalling the future is what releases whoever is blocked in
                // await(), and that thread may then finish the program. So the
                // worker must be COMPLETELY done -- VM destroyed, exception
                // object destroyed, mutator scope ended -- before it signals.
                // Setting the future from inside those scopes let main return
                // and tear down the CycleCollector while this thread was still
                // unwinding through it, which faulted at a garbage address,
                // intermittently and often after the program's last line.
                // Do not intern long strings on this thread. The pool is
                // thread_local but the strings it hands out escape to whoever
                // awaits us, so tearing the pool down at thread exit crashed
                // the process. See ValueImpl.h.
                g_stringInternEnabled = false;

                bool   signalResult = false;
                bool   failed       = false;
                Value  result;
                std::string errorText;

                {
                    struct MutatorScope {
                        MutatorScope()  { CycleCollector::instance().beginMutatorThread(); }
                        ~MutatorScope() { CycleCollector::instance().endMutatorThread(); }
                    } mutatorScope;

                    try {
                        auto threadVM = std::make_shared<BytecodeVM>(globalEnv);
                        threadVM->traceExecution = false;

                        threadVM->taskFuture = ezFut;
                        // NOTE: deliberately NOT setting isAsyncTask here.
                        // It looks attractive -- it suppresses the traceback the
                        // worker prints for a failure that the future already
                        // carries -- but it also changes WHICH exception the VM
                        // throws: the isAsyncTask path throws a RuntimeError
                        // carrying a live exception INSTANCE, and moving that
                        // GC-tracked object across the worker's catch and
                        // teardown faulted roughly one run in three, on
                        // addresses that decoded to native method names
                        // ("size", "init", "remove") -- a corrupted class table.
                        // Quieter output is not worth an intermittent crash.
                        result = threadVM->callFunction(closedFunc, closedArgs, 0, "native");

                        // A yielded VM is resumed by the event loop, which
                        // settles the future itself; settling it here too would
                        // publish a half-finished result.
                        signalResult = !threadVM->isYielded;
                    } catch(std::exception& e) {
                        // Record the failure ON THE FUTURE. This used to call
                        // set(Value()), completing the future SUCCESSFULLY with
                        // nil -- so await() returned nil and the caller had no
                        // way to learn the task had thrown. Anything built on
                        // "await rethrows" (allSettled, retry, timeouts) could
                        // not work. setError() makes get() rethrow, the same
                        // path cancel() already uses.
                        failed = true;
                        errorText = e.what();
                    }
                }

                EventLoop::instance().release();

                if (failed) {
                    ezFut->setError(errorText);
                } else if (signalResult) {
                    ezFut->set(result);
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

    interp.defineGlobal("cancel", Value::makeNativeFunction("cancel", 1, 
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isFuture()) { interp.runtimeError("cancel() expects future", 0, ""); return Value(); }
            args[0].asFuture()->cancel();
            return Value();
        }));

    // isDone(future) -> bool
    // Has it finished, without blocking? Reporting progress over a batch of
    // futures otherwise means racing a watcher thread against a zero-length
    // timeout, which answers "not yet" for work that already finished.
    interp.defineGlobal("isDone", Value::makeNativeFunction("isDone", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isFuture()) {
                interp.runtimeError("isDone() expects a future", 0, "");
                return Value();
            }
            return Value(args[0].asFuture()->isReady());
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

            // Snapshot once. The index identifying the winning future refers to
            // the sequence the wait was set up over, so indexing back into the
            // live array afterwards would pick the wrong future if another
            // thread mutated it in the meantime.
            std::vector<Value> futures = arr.getElementsCopy();
            for (auto& v : futures) {
                if (!v.isFuture()) { interp.runtimeError("awaitAny() array must contain only futures", 0, ""); return Value(); }
            }

#ifdef _WIN32
            std::vector<HANDLE> handles;
            handles.reserve(futures.size());
            for (auto& v : futures) {
                handles.push_back(v.asFuture()->hEvent);
            }

            // Plain `false`, not FALSE: the Win32 TRUE/FALSE macros are #undef'd
            // project-wide because they collide with TokenKind::TRUE/FALSE.
            DWORD result = WaitForMultipleObjects((DWORD)handles.size(), handles.data(), false, INFINITE);
            if (result >= WAIT_OBJECT_0 && result < WAIT_OBJECT_0 + handles.size()) {
                size_t index = result - WAIT_OBJECT_0;
                return futures[index].asFuture()->get();
            }
#else
            // No WaitForMultipleObjects off Windows, and each EZFuture owns its
            // own condition variable, so there is no single object to wait on.
            // Register a completion callback on every future that signals one
            // shared condvar instead -- this blocks rather than polls. then()
            // runs the callback immediately when that future has already
            // resolved, so a future that finished before we got here is picked
            // up without waiting. The captured shared_ptrs keep the shared state
            // alive for callbacks that never fire.
            auto mtx   = std::make_shared<std::mutex>();
            auto cv    = std::make_shared<std::condition_variable>();
            auto fired = std::make_shared<bool>(false);

            for (auto& v : futures) {
                v.asFuture()->then([mtx, cv, fired]() {
                    { std::lock_guard<std::mutex> lk(*mtx); *fired = true; }
                    cv->notify_all();
                });
            }
            {
                std::unique_lock<std::mutex> lk(*mtx);
                cv->wait(lk, [&fired] { return *fired; });
            }
            for (auto& v : futures) {
                if (v.asFuture()->isReady()) return v.asFuture()->get();
            }
#endif

            interp.runtimeError("awaitAny() failed to wait", 0, "");
            return Value();
        }));



    // Legacy std::filesystem C++ built-ins fully extracted into pure native EZ lib/fs.ez via FFI.

    // Legacy OS Operations extracted to lib/os.ez via FFI
        
}
