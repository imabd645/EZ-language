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

void registerDictBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("keys", Value::makeNativeFunction("keys", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isDictionary()) { interp.runtimeError("keys() expects dictionary as first argument", 0, ""); return Value(); }
                auto dictPtr = args[0].asDictionaryPtr();
                std::vector<Value> keys;
                for (const auto& kv : dictPtr->getMapCopy()) keys.push_back(Value(kv.first));
                return Value::makeArray(keys);
            }));

    interp.defineGlobal("values", Value::makeNativeFunction("values", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isDictionary()) { interp.runtimeError("values() expects dictionary as first argument", 0, ""); return Value(); }
                auto dictPtr = args[0].asDictionaryPtr();
                std::vector<Value> vals;
                for (const auto& kv : dictPtr->getMapCopy()) vals.push_back(kv.second);
                return Value::makeArray(vals);
            }));

    interp.defineGlobal("has_key", Value::makeNativeFunction("has_key", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isDictionary()) { interp.runtimeError("has_key() expects dictionary as first argument", 0, ""); return Value(); }
                std::string key = args[1].toString();
                auto dictPtr = args[0].asDictionaryPtr();
                return Value(dictPtr->has(key));
            }));

    interp.defineGlobal("dictRemove", Value::makeNativeFunction("dictRemove", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isDictionary()) { interp.runtimeError("dictRemove() expects dictionary", 0, ""); return Value(); }
                std::string key = args[1].toString();
                auto dictPtr = args[0].asDictionaryPtr();
                dictPtr->erase(key);
                return args[0];
            }));

}
