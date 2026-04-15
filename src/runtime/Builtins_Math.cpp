#include "../Builtins.h"
#include "../Interpreter.h"
#include <cmath>
#include <cstdlib>

void registerMathBuiltins(Interpreter& interp) {
    // floor(x)
    interp.defineGlobal("floor", Value::makeNativeFunction("floor", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                 interp.runtimeError("floor() expects number", 0, ""); return Value();
             }
            return Value(std::floor(args[0].asNumber()));
        }));
    
    // ceil(x)
    interp.defineGlobal("ceil", Value::makeNativeFunction("ceil", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                 interp.runtimeError("ceil() expects number", 0, ""); return Value();
             }
            return Value(std::ceil(args[0].asNumber()));
        }));
    
    // abs(x)
    interp.defineGlobal("abs", Value::makeNativeFunction("abs", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                 interp.runtimeError("abs() expects number", 0, ""); return Value();
             }
            return Value(std::abs(args[0].asNumber()));
        }));
    
    // sqrt(x)
    interp.defineGlobal("sqrt", Value::makeNativeFunction("sqrt", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
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
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("pow() expects two numbers", 0, ""); return Value();
             }
            return Value(std::pow(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // rand() - random number 0-1
    interp.defineGlobal("rand", Value::makeNativeFunction("rand", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            return Value(static_cast<double>(std::rand()) / RAND_MAX);
        }));
    
    // randint(min, max) - random integer in range
    interp.defineGlobal("randint", Value::makeNativeFunction("randint", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("randint() expects two numbers", 0, ""); return Value();
             }
            int min = static_cast<int>(args[0].asNumber());
            int max = static_cast<int>(args[1].asNumber());
            return Value(static_cast<double>(min + std::rand() % (max - min + 1)));
        }));
    
    // round(x)
    interp.defineGlobal("round", Value::makeNativeFunction("round", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                 interp.runtimeError("round() expects number", 0, ""); return Value();
             }
            return Value(std::round(args[0].asNumber()));
        }));
    
    // min(a, b)
    interp.defineGlobal("min", Value::makeNativeFunction("min", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("min() expects two numbers", 0, ""); return Value();
             }
            return Value(std::min(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // max(a, b)
    interp.defineGlobal("max", Value::makeNativeFunction("max", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                 interp.runtimeError("max() expects two numbers", 0, ""); return Value();
             }
            return Value(std::max(args[0].asNumber(), args[1].asNumber()));
        }));
}
