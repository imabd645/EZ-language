#include "vm/BytecodeVM.h"
#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "runtime/Environment.h"
#include <thread>
#include <chrono>
#include <iostream>
#include "eventloop/EventLoop.h"
#include "runtime/EZFuture.h"
#include "runtime/EZChannel.h"
#include <uv.h>

struct TimerContext {
    uv_timer_t timer;
    std::shared_ptr<EZFuture> fut;
};

void registerConcurrencyBuiltins(RuntimeContext& interp) {
    // mutex()
    interp.defineGlobal("mutex", Value::makeNativeFunction("mutex", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value(std::make_shared<EZMutex>());
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

            // Acquiring counts as a safe region: a thread waiting here holds no
            // EZ state and is not running bytecode, so the collector must not
            // wait for it to reach a backward jump -- it will not reach one
            // until it gets the lock. Without this, a thread that parked at a
            // safepoint while HOLDING this mutex left every other contender
            // blocked and unable to answer, so each collection burned its full
            // timeout and a lock-heavy workload crawled.
            {
                GCSafeRegion safe;
                mtx->mtx.lock();
            }
            // Unlock on every exit path, including an exception from the body.
            struct Unlocker {
                std::recursive_mutex& m;
                ~Unlocker() { m.unlock(); }
            } unlocker{mtx->mtx};

            return interp.callFunction(args[1], {}, 0, "");
        }));

    // wait(ms)
    interp.defineGlobal("wait", Value::makeNativeFunction("wait", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() && !args[0].isInteger()) {
                interp.runtimeError("wait() expects milliseconds (number)", 0, "");
                return Value();
            }
            // Fix 1.4: use the correct accessor — asInteger() truncates floats silently
            int ms = args[0].isInteger()
                ? static_cast<int>(args[0].asInteger())
                : static_cast<int>(args[0].asFloat());
            if (ms > 0) {
                // Sleeping touches no EZ object, so count as parked: otherwise a
                // worker asleep here never reaches a backward jump and every
                // collection times out waiting for it.
                GCSafeRegion safe;
                std::this_thread::sleep_for(std::chrono::milliseconds(ms));
            }
            return Value();
        }));

    // waitAsync(ms) - Returns a Future that resolves after ms milliseconds
    interp.defineGlobal("waitAsync", Value::makeNativeFunction("waitAsync", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                interp.runtimeError("waitAsync() expects milliseconds (number)", 0, "");
                return Value();
            }
            int ms = static_cast<int>(args[0].asInteger());
            
            auto fut = std::make_shared<EZFuture>();
            
            if (ms > 0) {
                EventLoop::instance().pushTask([ms, fut]() {
                    EventLoop::instance().retain();
                    TimerContext* ctx = new TimerContext();
                    ctx->fut = fut;
                    
                    uv_timer_init(EventLoop::instance().getLoop(), &ctx->timer);
                    ctx->timer.data = ctx;
                    
                    uv_timer_start(&ctx->timer, [](uv_timer_t* handle) {
                        TimerContext* ctx = static_cast<TimerContext*>(handle->data);
                        ctx->fut->set(Value(true));
                        
                        uv_close(reinterpret_cast<uv_handle_t*>(handle), [](uv_handle_t* handle) {
                            TimerContext* ctx = static_cast<TimerContext*>(handle->data);
                            delete ctx;
                            EventLoop::instance().release();
                        });
                    }, ms, 0);
                });
            } else {
                fut->set(Value(true));
            }
            
            return Value::makeFuture(fut);
        }));

    // class Atomic
    auto atomicClass = std::make_shared<EZClass>("Atomic");
    CycleCollector::instance().track(atomicClass, ValueType::CLASS);
    
    // Atomic.init(initial)
    atomicClass->setMethod("init", Value::makeNativeFunction("init", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            // args[0] is the instance, args[1] is the initial value
            auto instance = args[0].asInstance();
            long long initial = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            instance->setProperty("_atomic", Value::makeAtomic(initial));
            return args[0];
        }));
        
    atomicClass->setMethod("get", Value::makeNativeFunction("get", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            return Value(atomic->val.load());
        }));
        
    atomicClass->setMethod("set", Value::makeNativeFunction("set", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            atomic->val.store(v);
            return Value(v);
        }));
        
    atomicClass->setMethod("add", Value::makeNativeFunction("add", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            return Value(atomic->val.fetch_add(v) + v);
        }));
        
    atomicClass->setMethod("sub", Value::makeNativeFunction("sub", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto atomic = instance->getProperty("_atomic").asAtomicPtr();
            long long v = args[1].isNumber() ? static_cast<long long>(args[1].asNumber()) : 0;
            return Value(atomic->val.fetch_sub(v) - v);
        }));
        
    interp.defineGlobal("Atomic", Value(atomicClass));

    // class Channel
    auto channelClass = std::make_shared<EZClass>("Channel");
    CycleCollector::instance().track(channelClass, ValueType::CLASS);
    
    // Channel.init()
    channelClass->setMethod("init", Value::makeNativeFunction("init", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            instance->setProperty("_channel", Value::makeChannel(std::make_shared<EZChannel>()));
            return args[0];
        }));
        
    channelClass->setMethod("send", Value::makeNativeFunction("send", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            {
                std::lock_guard<std::mutex> guard(chan->mtx);
                if (chan->closed) {
                    interp.runtimeError("Cannot send on closed channel", 0, "");
                    return Value();
                }
                chan->q.push(args[1]);
            }
            chan->cv.notify_one();
            return Value(true);
        }));
        
    channelClass->setMethod("receive", Value::makeNativeFunction("receive", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            
            GCSafeRegion safe;   // blocked on a producer, not on the heap
            std::unique_lock<std::mutex> lock(chan->mtx);
            chan->cv.wait(lock, [&]() {
                return !chan->q.empty() || chan->closed;
            });
            
            if (!chan->q.empty()) {
                Value v = chan->q.front();
                chan->q.pop();
                return v;
            }
            return Value(); // Return nil if closed and empty
        }));
        
    channelClass->setMethod("close", Value::makeNativeFunction("close", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            {
                std::lock_guard<std::mutex> guard(chan->mtx);
                chan->closed = true;
                chan->cv.notify_all();
            }
            return Value(true);
        }));

    // Channel.tryReceive() -> value | nil
    // Takes a value only if one is already queued. Returns nil rather than
    // blocking, which is what a non-blocking acquire needs: without it a
    // semaphore has to poll, and polling a shared counter is exactly the
    // check-then-act race that makes such a semaphore grant too many permits.
    // A queued nil is indistinguishable from "empty" -- send a token, not nil.
    channelClass->setMethod("tryReceive", Value::makeNativeFunction("tryReceive", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            std::lock_guard<std::mutex> guard(chan->mtx);
            if (chan->q.empty()) return Value();
            Value v = chan->q.front();
            chan->q.pop();
            return v;
        }));

    // Channel.receiveTimeout(ms) -> value | nil
    // Blocks up to `ms` milliseconds. Returns nil on timeout, so a bounded wait
    // needs no polling loop.
    channelClass->setMethod("receiveTimeout", Value::makeNativeFunction("receiveTimeout", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            if (args.size() < 2 || (!args[1].isNumber() && !args[1].isInteger())) {
                interp.runtimeError("receiveTimeout() expects milliseconds (number)", 0, "");
                return Value();
            }
            long long ms = args[1].isInteger()
                ? static_cast<long long>(args[1].asInteger())
                : static_cast<long long>(args[1].asFloat());
            if (ms < 0) ms = 0;

            GCSafeRegion safe;   // bounded block on a producer
            std::unique_lock<std::mutex> lock(chan->mtx);
            bool ready = chan->cv.wait_for(lock, std::chrono::milliseconds(ms), [&]() {
                return !chan->q.empty() || chan->closed;
            });
            if (!ready) return Value();          // timed out
            if (!chan->q.empty()) {
                Value v = chan->q.front();
                chan->q.pop();
                return v;
            }
            return Value();                      // closed and drained
        }));

    // Channel.size() -> integer   (values currently queued)
    channelClass->setMethod("size", Value::makeNativeFunction("size", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            std::lock_guard<std::mutex> guard(chan->mtx);
            return Value(static_cast<long long>(chan->q.size()));
        }));

    // Channel.isClosed() -> bool
    channelClass->setMethod("isClosed", Value::makeNativeFunction("isClosed", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto chan = instance->getProperty("_channel").asChannelPtr();
            std::lock_guard<std::mutex> guard(chan->mtx);
            return Value(chan->closed);
        }));

    interp.defineGlobal("Channel", Value(channelClass));

    interp.defineGlobal("spawn", Value::makeNativeFunction("spawn", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isCallable()) { interp.runtimeError("spawn() expects function", 0, ""); return Value(); }
            
            Value func = args[0];
            bool isDaemon = false;
            size_t endIdx = args.size();
            
            if (args.size() > 1) {
                const Value& lastArg = args.back();
                if (lastArg.isDictionary()) {
                    auto dict = lastArg.asDictionaryPtr();
                    if (dict->has("daemon")) {
                        Value dVal = dict->get("daemon");
                        if (dVal.isBool() && dVal.asBool()) {
                            isDaemon = true;
                            endIdx--; 
                        }
                    }
                } else if (lastArg.isBool() && lastArg.asBool()) {
                    isDaemon = true;
                    endIdx--; 
                }
            }
            
            std::vector<Value> fnArgs(args.begin() + 1, args.begin() + endIdx);
            auto globalEnv = interp.getGlobalEnv();

            std::unordered_map<void*, Value> seen;

            std::function<Value(const Value&)> closeUpvals = [&](const Value& v) -> Value {
                if (v.isClosure()) {
                    auto oldCl = v.asClosure();
                    if (seen.count(oldCl.get())) return seen[oldCl.get()];

                    auto newCl = std::make_shared<EZClosure>(oldCl->function);
                    Value newClVal = Value::makeClosure(newCl);
                    seen[oldCl.get()] = newClVal;

                    for (auto& uv : oldCl->upvalues) {
                        if (!uv) { newCl->upvalues.push_back(nullptr); continue; }
                        auto newUv = std::make_shared<UpvalueObj>();
                        Value* loc = uv->location.load();
                        Value snap = (loc != nullptr) ? *loc : Value();
                        newUv->closed = closeUpvals(snap);
                        newUv->location.store(&newUv->closed);
                        newUv->next = nullptr;
                        newCl->upvalues.push_back(newUv);
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
                return v;
            };

            Value closedFunc = closeUpvals(func);
            std::vector<Value> closedArgs;
            for (auto& a : fnArgs) closedArgs.push_back(closeUpvals(a));

            auto ezFut = std::make_shared<EZFuture>();

            if (!isDaemon) {
                EventLoop::instance().retain();
            }
            std::thread([ezFut, globalEnv, closedFunc, closedArgs, isDaemon]() {
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
                        threadVM->isAsyncTask = true;
                        threadVM->isWorkerThread = true;
                        result = threadVM->callFunction(closedFunc, closedArgs, 0, "native");

                        signalResult = !threadVM->isYielded;
                    } catch(std::exception& e) {
                        failed = true;
                        errorText = e.what();
                    }
                }

                if (failed) {
                    ezFut->setError(errorText);
                } else if (signalResult) {
                    ezFut->set(result);
                }

                if (!isDaemon) {
                    EventLoop::instance().release();
                }
            }).detach();

            return Value::makeFuture(ezFut);
        }));

    auto awaitFn = [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isFuture()) { interp.runtimeError("await() expects future", 0, ""); return Value(); }
        auto fut = args[0].asFuture();
        {
            GCSafeRegion safe;
            fut->wait();
        }
        if (fut->isError()) {
            interp.throwException("Exception", fut->getError());
            return Value();
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
                if (fut->isError()) {
                    interp.throwException("Exception", fut->getError());
                    return Value();
                }
                results.push_back(fut->get());
            }
            return Value::makeArray(results);
        }));

    interp.defineGlobal("awaitAny", Value::makeNativeFunction("awaitAny", 1, 
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("awaitAny() expects array of futures", 0, ""); return Value(); }
            auto& arr = args[0].asArray();
            if (arr.empty()) { interp.runtimeError("awaitAny() cannot accept empty array", 0, ""); return Value(); }

            std::vector<Value> futures = arr.getElementsCopy();
            for (auto& v : futures) {
                if (!v.isFuture()) { interp.runtimeError("awaitAny() array must contain only futures", 0, ""); return Value(); }
            }

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
                if (v.asFuture()->isReady()) {
                    auto fut = v.asFuture();
                    if (fut->isError()) {
                        interp.throwException("Exception", fut->getError());
                        return Value();
                    }
                    return fut->get();
                }
            }

            interp.runtimeError("awaitAny() failed to wait", 0, "");
            return Value();
        }));
}

