#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "builtins/Builtins.h"   // ezReapDeadFileStreams()
#include "runtime/RuntimeContext.h"
#include "runtime/Value.h"
#include "runtime/Environment.h"
#include <vector>

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
            interp.releaseStaleStackSlots();
            CycleCollector::instance().collect();
            ezReapDeadFileStreams();
            return Value(true);
        }));

    interp.defineGlobal("gc_tracked", Value::makeNativeFunction("gc_tracked", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            return Value((long long)CycleCollector::instance().trackedCount());
        }));

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
}
