#include "runtime/objects/EZObjects.h"
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
            
            // RAII lock â€” automatically unlocks on scope exit, even if an exception is thrown
            std::lock_guard<std::recursive_mutex> guard(mtx->mtx);
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
            } else {
                fut->set(Value(true));
            }
            
            return Value::makeFuture(fut);
        }));

    // class Atomic
    auto atomicClass = std::make_shared<EZClass>("Atomic");
    
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

    interp.defineGlobal("Channel", Value(channelClass));
}
