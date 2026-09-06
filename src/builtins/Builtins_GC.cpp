#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"   // ezReapDeadFileStreams()
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
#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

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
            // Dead stack slots still reference whatever they last held, which
            // would make genuinely unreachable objects look live to a
            // collection the user explicitly asked for.
            interp.releaseStaleStackSlots();
            CycleCollector::instance().collect();
            // Collecting may have destroyed File instances. Their OS handles
            // live in a side table that is otherwise only pruned when another
            // file is opened, so release them here too -- a program that drops
            // its last File and never opens another would hold the handle
            // indefinitely.
            ezReapDeadFileStreams();
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

    interp.defineGlobal("__gc_is_enabled", Value::makeNativeFunction("__gc_is_enabled", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            return Value(CycleCollector::instance().isEnabled());
        }));

    interp.defineGlobal("__gc_stats", Value::makeNativeFunction("__gc_stats", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            Value dictVal = Value::makeDictionary();
            auto dict = dictVal.asDictionaryPtr();
            dict->modifyMap([&](auto& m) {
                m["tracked"] = Value((long long)CycleCollector::instance().trackedCount());
                m["cycles"] = Value((long long)CycleCollector::instance().cyclesCollected());
                m["enabled"] = Value(CycleCollector::instance().isEnabled());
            });
            return dictVal;
        }));

    interp.defineGlobal("__vm_get_max_recursion_depth", Value::makeNativeFunction("__vm_get_max_recursion_depth", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getMaxRecursionDepth());
        }));

    interp.defineGlobal("__vm_set_max_recursion_depth", Value::makeNativeFunction("__vm_set_max_recursion_depth", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                interp.runtimeError("__vm_set_max_recursion_depth() expects a number", 0, "");
                return Value();
            }
            long long depth = (long long)args[0].asNumber();
            if (depth < 1) depth = 1;
            interp.setMaxRecursionDepth((size_t)depth);
            return Value(true);
        }));

    interp.defineGlobal("__vm_current_depth", Value::makeNativeFunction("__vm_current_depth", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getCallDepth());
        }));

    interp.defineGlobal("__vm_instruction_count", Value::makeNativeFunction("__vm_instruction_count", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)interp.getInstructionCount());
        }));

    interp.defineGlobal("__vm_reset_instruction_count", Value::makeNativeFunction("__vm_reset_instruction_count", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            interp.resetInstructionCount();
            return Value(true);
        }));

    interp.defineGlobal("__vm_get_max_instructions", Value::makeNativeFunction("__vm_get_max_instructions", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value(static_cast<double>(interp.getMaxInstructions()));
        }));

    interp.defineGlobal("__vm_set_max_instructions", Value::makeNativeFunction("__vm_set_max_instructions", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isNumber()) {
                interp.runtimeError("__vm_set_max_instructions expects a number", 0, "");
                return Value();
            }
            interp.setMaxInstructions(static_cast<uint64_t>(args[0].asNumber()));
            return Value(true);
        }));

    interp.defineGlobal("__stack_trace", Value::makeNativeFunction("__stack_trace", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value::makeArray(interp.getStackTraceFrames());
        }));

    interp.defineGlobal("__engine_info", Value::makeNativeFunction("__engine_info", 0,
        [](RuntimeContext&, const std::vector<Value>&) -> Value {
            Value dictVal = Value::makeDictionary();
            auto dict = dictVal.asDictionaryPtr();
            dict->modifyMap([&](auto& m) {
                m["version"] = Value("5.0.0");
                m["engine"] = Value("EZ Bytecode VM");
#if defined(_WIN32) || defined(_WIN64)
                m["platform"] = Value("windows");
#elif defined(__APPLE__)
                m["platform"] = Value("macos");
#elif defined(__linux__)
                m["platform"] = Value("linux");
#else
                m["platform"] = Value("unknown");
#endif
#if defined(__x86_64__) || defined(_M_X64)
                m["arch"] = Value("x86_64");
#elif defined(__aarch64__) || defined(_M_ARM64)
                m["arch"] = Value("arm64");
#else
                m["arch"] = Value("x86");
#endif
                m["pointer_size"] = Value(static_cast<double>(sizeof(void*)));
                m["endianness"] = Value("little");
#if defined(__clang__)
                m["compiler"] = Value("Clang " + std::to_string(__clang_major__) + "." + std::to_string(__clang_minor__));
#elif defined(__GNUC__)
                m["compiler"] = Value("GCC " + std::to_string(__GNUC__) + "." + std::to_string(__GNUC_MINOR__));
#elif defined(_MSC_VER)
                m["compiler"] = Value("MSVC " + std::to_string(_MSC_VER));
#else
                m["compiler"] = Value("unknown");
#endif
            });
            return dictVal;
        }));

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
    // throwException() only builds a typed instance when a global class of that
    // name exists; otherwise it silently falls back to a plain string error that
    // no `catch (SomeError e)` clause can match. These three were named at
    // throw sites but never defined, so file, parser and regex failures all
    // arrived as untyped strings.
    makeErrorClass("IOError", exceptionClass);
    makeErrorClass("SyntaxError", exceptionClass);
    makeErrorClass("RegexError", exceptionClass);

    // The faults the VM itself raises. Each was previously an untyped string,
    // so `catch (e)` saw only a message and no program could tell an index
    // mistake from a missing method -- the two need entirely different fixes.
    //
    // ArithmeticError is the parent of the numeric faults so that a caller
    // guarding a calculation can catch the family in one clause rather than
    // listing every member.
    auto arithmeticClass = makeErrorClass("ArithmeticError", exceptionClass);
    makeErrorClass("ZeroDivisionError", arithmeticClass);
    makeErrorClass("OverflowError", arithmeticClass);

    makeErrorClass("AttributeError", exceptionClass);
    makeErrorClass("NameError", exceptionClass);
    makeErrorClass("RecursionError", exceptionClass);
    makeErrorClass("NotImplementedError", exceptionClass);
    makeErrorClass("TimeoutError", exceptionClass);
    makeErrorClass("AssertionError", exceptionClass);
    // Raised by the compiler, not the VM -- a `use` is resolved before the
    // program runs, so this cannot be caught. It is defined anyway so the name
    // resolves and `throw ModuleNotFoundError(...)` from EZ code works.
    makeErrorClass("ModuleNotFoundError", exceptionClass);

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
                    // makeClosure() registers it with the collector; the raw
                    // Value() constructor does not. Track ONCE and reuse the
                    // same Value below, so the copy handed to the worker is
                    // collectable but not double-registered.
                    Value newClVal = Value::makeClosure(newCl);
                    seen[oldCl.get()] = newClVal;

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
                    return newClVal;
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
                // Long strings are not interned on this thread: interning is
                // opt-in and only the main thread opts in, so nothing has to be
                // switched off here. See Runtime.cpp.

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
                        // The failure is carried by the future for whoever
                        // awaits it, so the worker must not ALSO dump a
                        // traceback to stderr. Without this, handling failures
                        // as data -- allSettled over a batch, a retry loop --
                        // prints a full traceback per expected failure, and
                        // even `try { await(f) } catch (e) {}` reports a crash
                        // it already handled. Same choice the `async task`
                        // path makes.
                        threadVM->isAsyncTask = true;
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

                // Settle the future BEFORE releasing our EventLoop refcount, not
                // after. set()/setError() synchronously runs every then()
                // callback on THIS thread, and the await-side callback's whole
                // job is to push the resume task onto the EventLoop's queue.
                // release() also wakes the loop (to re-check its exit
                // condition), so calling it first opened a race: the main
                // thread could wake on release()'s signal, find the queue
                // still empty (this thread hadn't reached set() yet) and
                // pendingIoCount already at 0, and stop the loop entirely --
                // after which set()'s pushTask() lands in a queue nobody is
                // draining anymore, and the awaiting script never resumes.
                // Settling first guarantees the resume task is already queued
                // by the time release()'s wake-up is observed.
                if (failed) {
                    ezFut->setError(errorText);
                } else if (signalResult) {
                    ezFut->set(result);
                }

                EventLoop::instance().release();
            }).detach();

            return Value::makeFuture(ezFut);
        }));

    auto awaitFn = [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isFuture()) { interp.runtimeError("await() expects future", 0, ""); return Value(); }
        auto fut = args[0].asFuture();
        {
            // Blocked on another thread's result: nothing of ours is in flight.
            GCSafeRegion safe;
            fut->wait();
        }
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
                { GCSafeRegion safe; fut->wait(); }
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

            // Register a completion callback on every future that signals one
            // shared condvar. This blocks without polling, supports any number
            // of futures (lifting the Win32 WaitForMultipleObjects 64-handle limit),
            // and works across all platforms. then() runs immediately if a future
            // is already resolved.
            auto mtx   = std::make_shared<std::mutex>();
            auto cv    = std::make_shared<std::condition_variable>();
            auto fired = std::make_shared<bool>(false);

            for (auto& v : futures) {
                v.asFuture()->then([mtx, cv, fired]() {
                    {
                        std::lock_guard<std::mutex> lk(*mtx);
                        *fired = true;
                    }
                    cv->notify_all();
                });
            }
            {
                GCSafeRegion safe;
                std::unique_lock<std::mutex> lk(*mtx);
                cv->wait(lk, [&fired] { return *fired; });
            }
            for (auto& v : futures) {
                if (v.asFuture()->isReady()) return v.asFuture()->get();
            }

            interp.runtimeError("awaitAny() failed to wait", 0, "");
            return Value();
        }));



    // Legacy std::filesystem C++ built-ins fully extracted into pure native EZ lib/fs.ez via FFI.

    // Legacy OS Operations extracted to lib/os.ez via FFI
        
}
