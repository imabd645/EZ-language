#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include <cmath>
#include <cstdlib>
#include <random>

static std::mt19937_64& ezGetRNG() {
    thread_local std::mt19937_64 rng([]() {
        std::random_device rd;
        return (static_cast<uint64_t>(rd()) << 32) ^ rd();
    }());
    return rng;
}

void registerMathBuiltins(RuntimeContext& interp) {
    // floor(x)
    interp.defineGlobal("floor", Value::makeNativeFunction("floor", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isInteger()) return args[0];
            if (!args[0].isNumber()) {
                 interp.runtimeError("floor() expects number", 0, ""); return Value();
             }
            return Value(static_cast<long long>(std::floor(args[0].asNumber())));
        }));
    
    // ceil(x)
    interp.defineGlobal("ceil", Value::makeNativeFunction("ceil", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isInteger()) return args[0];
            if (!args[0].isNumber()) {
                 interp.runtimeError("ceil() expects number", 0, ""); return Value();
             }
            return Value(static_cast<long long>(std::ceil(args[0].asNumber())));
        }));
    
    // abs(x)
    interp.defineGlobal("abs", Value::makeNativeFunction("abs", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isInteger()) return Value(std::abs(args[0].asInteger()));
            if (!args[0].isNumber()) {
                 interp.runtimeError("abs() expects number", 0, ""); return Value();
             }
            return Value(std::abs(args[0].asNumber()));
        }));
    
    // sqrt(x)
    interp.defineGlobal("sqrt", Value::makeNativeFunction("sqrt", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() && !args[0].isInteger()) {
                 interp.runtimeError("sqrt() expects number", 0, ""); return Value();
             }
            double val = args[0].asNumber();
            if (val < 0) {
                 interp.runtimeError("sqrt() of negative number", 0, ""); return Value();
             }
            return Value(std::sqrt(val));
        }));
    
    // pow(base, exp)
    interp.defineGlobal("pow", Value::makeNativeFunction("pow", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if ((!args[0].isNumber() && !args[0].isInteger()) || (!args[1].isNumber() && !args[1].isInteger())) {
                 interp.runtimeError("pow() expects two numbers", 0, ""); return Value();
             }
            return Value(std::pow(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // rand() - random number 0-1
    interp.defineGlobal("rand", Value::makeNativeFunction("rand", 0,
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            std::uniform_real_distribution<double> dist(0.0, 1.0);
            return Value(dist(ezGetRNG()));
        }));

    // randint(min, max) - random integer in range
    interp.defineGlobal("randint", Value::makeNativeFunction("randint", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() && !args[0].isInteger()) {
                 interp.runtimeError("randint() expects two numbers", 0, ""); return Value();
             }
            long long min = args[0].asInteger();
            long long max = args[1].asInteger();
            if (max < min) return Value(min);
            std::uniform_int_distribution<long long> dist(min, max);
            return Value(dist(ezGetRNG()));
        }));
    
    // round(x)
    interp.defineGlobal("round", Value::makeNativeFunction("round", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isInteger()) return args[0];
            if (!args[0].isNumber()) {
                 interp.runtimeError("round() expects number", 0, ""); return Value();
             }
            return Value(static_cast<long long>(std::round(args[0].asNumber())));
        }));
    
    // min(a, b)
    interp.defineGlobal("min", Value::makeNativeFunction("min", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("min() expects two numbers", 0, ""); return Value();
             }
            if (args[0].isInteger() && args[1].isInteger()) {
                return Value(std::min(args[0].asInteger(), args[1].asInteger()));
            }
            return Value(std::min(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // max(a, b)
    interp.defineGlobal("max", Value::makeNativeFunction("max", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("max() expects two numbers", 0, ""); return Value();
             }
            if (args[0].isInteger() && args[1].isInteger()) {
                return Value(std::max(args[0].asInteger(), args[1].asInteger()));
            }
            return Value(std::max(args[0].asNumber(), args[1].asNumber()));
        }));
}
