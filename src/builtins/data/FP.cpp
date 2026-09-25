#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Utf8.h"
#include "utils/MiniJson.h"
#include <stdexcept>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

void registerFPBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("map", Value::makeNativeFunction("map", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("map() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("map() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                std::vector<Value> result;
                for (const auto& elem : arr.getElementsCopy()) result.push_back(interp.callFunction(args[1], {elem}, 0, "native"));
                return Value::makeArray(result);
            }));

    interp.defineGlobal("filter", Value::makeNativeFunction("filter", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("filter() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("filter() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                std::vector<Value> result;
                for (const auto& elem : arr.getElementsCopy()) {
                    Value test = interp.callFunction(args[1], {elem}, 0, "native");
                    if (test.isTruthy()) result.push_back(elem);
                }
                return Value::makeArray(result);
            }));

    interp.defineGlobal("reduce", Value::makeNativeFunction("reduce", 3,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("reduce() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("reduce() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                Value acc = args[2];
                for (const auto& elem : arr.getElementsCopy()) acc = interp.callFunction(args[1], {acc, elem}, 0, "native");
                return acc;
            }));

    interp.defineGlobal("forEach", Value::makeNativeFunction("forEach", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("forEach() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("forEach() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr.getElementsCopy()) interp.callFunction(args[1], {elem}, 0, "native");
                return Value();
            }));

    interp.defineGlobal("find", Value::makeNativeFunction("find", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("find() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("find() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr.getElementsCopy()) {
                    Value test = interp.callFunction(args[1], {elem}, 0, "native");
                    if (test.isTruthy()) return elem;
                }
                return Value(); 
            }));

    interp.defineGlobal("every", Value::makeNativeFunction("every", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("every() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("every() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr.getElementsCopy()) {
                    Value test = interp.callFunction(args[1], {elem}, 0, "native");
                    if (!test.isTruthy()) return Value(false);
                }
                return Value(true);
            }));

    interp.defineGlobal("some", Value::makeNativeFunction("some", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("some() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isCallable()) { interp.runtimeError("some() expects function as second argument", 0, ""); return Value(); }
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr.getElementsCopy()) {
                    Value test = interp.callFunction(args[1], {elem}, 0, "native");
                    if (test.isTruthy()) return Value(true);
                }
                return Value(false);
            }));

}
