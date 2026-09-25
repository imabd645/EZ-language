#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "vm/BytecodeVM.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>

#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <unordered_map>
#include <memory>
#include "vm/BytecodeVM.h"

// ── Timer storage ──────────────────────────────────────────────────────────────
// Each Timer instance owns a background thread that sleeps and calls back.
// We store the control state in a shared struct so the thread and the EZ
// instance can communicate safely.

struct EZTimerState {
    std::atomic<bool> running{false};
    std::atomic<bool> stopRequested{false};
    int intervalMs = 0;
    bool repeat = false;
    Value callback;                     // the EZ callable
    std::shared_ptr<RuntimeContext> vm;  // not used — we store the raw ptr below
    RuntimeContext* interpPtr = nullptr;
};

static std::mutex g_timerMtx;
static std::unordered_map<EZInstance*, std::shared_ptr<EZTimerState>> g_timerStates;

static void storeTimerState(EZInstance* inst, std::shared_ptr<EZTimerState> st) {
    std::lock_guard<std::mutex> lk(g_timerMtx);
    g_timerStates[inst] = std::move(st);
}

static std::shared_ptr<EZTimerState> getTimerState(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_timerMtx);
    auto it = g_timerStates.find(inst);
    if (it != g_timerStates.end()) return it->second;
    return nullptr;
}

static void removeTimerState(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_timerMtx);
    g_timerStates.erase(inst);
}


void registerTimerBuiltins(RuntimeContext& interp) {
auto timerClass = std::make_shared<EZClass>("Timer");
    CycleCollector::instance().track(timerClass, ValueType::CLASS);

    // Timer.init(intervalMs, repeat=false)
    timerClass->setMethod("init", Value::makeNativeFunction("init", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (args.size() < 2 || !args[1].isNumber()) {
                interp.throwException("TypeError",
                    "Timer() expects interval in milliseconds as first argument", 0, "");
                return Value();
            }

            int intervalMs = static_cast<int>(args[1].asNumber());
            bool repeat = false;
            if (args.size() >= 3) {
                if (args[2].isBool()) repeat = args[2].asBool();
                else if (args[2].isNumber()) repeat = args[2].asNumber() != 0;
            }

            auto state = std::make_shared<EZTimerState>();
            state->intervalMs = intervalMs;
            state->repeat = repeat;
            storeTimerState(instance.get(), state);

            instance->setProperty("_interval", Value(static_cast<long long>(intervalMs)));
            instance->setProperty("_repeat", Value(repeat));
            instance->setProperty("_running", Value(false));
            return args[0];
        }));

    // Timer.onTick(callback)
    // Registers the callback function to fire on each tick.
    timerClass->setMethod("onTick", Value::makeNativeFunction("onTick", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isCallable()) {
                interp.throwException("TypeError", "Timer.onTick() expects a function", 0, "");
                return Value();
            }
            auto state = getTimerState(instance.get());
            if (!state) {
                interp.throwException("ValueError", "Timer not initialized", 0, "");
                return Value();
            }
            state->callback = args[1];
            return args[0]; // for chaining
        }));

    // Timer.start()
    // Launches a background thread that sleeps and invokes the callback.
    timerClass->setMethod("start", Value::makeNativeFunction("start", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto state = getTimerState(instance.get());
            if (!state) {
                interp.throwException("ValueError", "Timer not initialized", 0, "");
                return Value();
            }
            if (state->running.load()) {
                return Value(false); // already running
            }
            if (!state->callback.isCallable()) {
                interp.throwException("ValueError",
                    "Timer.start() requires a callback — call onTick() first", 0, "");
                return Value();
            }

            state->running.store(true);
            state->stopRequested.store(false);
            state->interpPtr = &interp;
            instance->setProperty("_running", Value(true));

            Value cb = state->callback;
            int ms = state->intervalMs;
            bool repeat = state->repeat;
            auto stateWeak = std::weak_ptr<EZTimerState>(state);

            auto globalEnv = interp.getGlobalEnv();

            std::thread([stateWeak, cb, ms, repeat, globalEnv]() {
                // Long strings are not interned on this thread: interning is
                // opt-in and only the main thread opts in. See Runtime.cpp.

                // Register as a mutator for the thread's whole life. The
                // callback runs EZ code that reads and writes shared objects,
                // so without this the collector had no idea this thread existed
                // and could walk the graph while a tick was reshaping it --
                // precisely the race that spawn()ed workers register to avoid.
                // Ticks answer safepoints at backward jumps like any other
                // bytecode, and the sleep below is a safe region.
                struct MutatorScope {
                    MutatorScope()  { CycleCollector::instance().beginMutatorThread(); }
                    ~MutatorScope() { CycleCollector::instance().endMutatorThread(); }
                } mutatorScope;

                while (true) {
                    {
                        // Idle between ticks: touches nothing, so the collector
                        // must not wait for this thread to reach a backward jump.
                        GCSafeRegion safe;
                        std::this_thread::sleep_for(std::chrono::milliseconds(ms));
                    }

                    auto st = stateWeak.lock();
                    if (!st || st->stopRequested.load()) break;

                    // Execute the callback on a fresh VM (thread-safe)
                    try {
                        auto threadVM = std::make_shared<BytecodeVM>(globalEnv);
                        threadVM->traceExecution = false;
                        threadVM->callFunction(cb, {}, 0, "timer");
                    } catch (const std::exception& e) {
                        std::cerr << "[Timer] callback error: " << e.what() << std::endl;
                    }

                    if (!repeat) break;
                }

                auto st = stateWeak.lock();
                if (st) {
                    st->running.store(false);
                }
            }).detach();

            return Value(true);
        }));

    // Timer.stop()
    timerClass->setMethod("stop", Value::makeNativeFunction("stop", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto state = getTimerState(instance.get());
            if (state) {
                state->stopRequested.store(true);
                state->running.store(false);
            }
            instance->setProperty("_running", Value(false));
            return Value(true);
        }));

    // Timer.isRunning() -> bool
    timerClass->setMethod("isRunning", Value::makeNativeFunction("isRunning", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto state = getTimerState(instance.get());
            if (!state) return Value(false);
            return Value(state->running.load());
        }));

    interp.defineGlobal("Timer", Value(timerClass));
}
