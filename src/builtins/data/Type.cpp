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

void registerTypeBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("str", Value::makeNativeFunction("str", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                return Value(interp.stringify(args[0]));
            }));

    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args[0].isNumber()) return args[0];
                if (args[0].isInteger()) return args[0];
                if (args[0].isString()) {
                    const std::string original = args[0].asString();
                    try {
                        // Trailing whitespace is fine; trailing anything else is not.
                        std::string s = original;
                        size_t end = s.find_last_not_of(" \t\r\n");
                        s = (end == std::string::npos) ? "" : s.substr(0, end + 1);
                        if (s.empty()) throw std::invalid_argument("empty");
    
                        size_t consumed = 0;
                        Value out;
                        if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
                            out = Value(std::stoll(s, &consumed));
                        } else {
                            out = Value(std::stod(s, &consumed));
                        }
                        // stoll/stod stop at the first character they cannot use and
                        // report success, so num("12abc") quietly returned 12 while
                        // num("abc") threw -- the same malformed input handled two
                        // different ways. Require the WHOLE string to be a number.
                        if (consumed != s.size()) throw std::invalid_argument("trailing characters");
                        return out;
                    }
                    catch (...) { interp.runtimeError("Cannot convert '" + original + "' to number", 0, ""); return Value(); }
                }
                if (args[0].isBool()) return Value(args[0].asBool() ? 1LL : 0LL);
                interp.runtimeError("Cannot convert " + args[0].typeName() + " to number", 0, ""); return Value();
             }));

    interp.defineGlobal("type", Value::makeNativeFunction("type", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                return Value(args[0].typeName());
            }));

    interp.defineGlobal("typeOf", Value::makeNativeFunction("typeOf", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                return Value(args[0].typeName());
            }));

    interp.defineGlobal("properties", Value::makeNativeFunction("properties", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isInstance()) { interp.runtimeError("properties() expects an instance object", 0, ""); return Value(); }
                auto instancePtr = args[0].asInstance();
                std::vector<Value> keys;
                for (const auto& kv : instancePtr->getPropertiesCopy()) keys.push_back(Value(kv.first));
                return Value::makeArray(keys);
            }));

    interp.defineGlobal("getattr", Value::makeNativeFunction("getattr", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[1].isString()) { interp.runtimeError("getattr() expects property name as string", 0, ""); return Value(); }
                std::string prop = args[1].asString();
                if (args[0].isDictionary()) {
                    // Was: `auto it = dictPtr->getMapCopy().find(prop);
                    //       if (it != dictPtr->getMapCopy().end()) return it->second;`
                    // Each getMapCopy() returns a TEMPORARY map that dies at the end of
                    // its full-expression, so `it` dangled and was then compared against
                    // end() of a *different* container -- undefined behaviour, plus two
                    // full O(n) map copies. One locked O(1) lookup instead.
                    return args[0].asDictionaryPtr()->get(prop);
                }
                if (args[0].isInstance()) {
                    return args[0].asInstance()->getProperty(prop);
                }
                interp.runtimeError("getattr() expects dictionary or instance object", 0, ""); return Value();
            }));

    interp.defineGlobal("setattr", Value::makeNativeFunction("setattr", 3,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[1].isString()) { interp.runtimeError("setattr() expects property name as string", 0, ""); return Value(); }
                std::string prop = args[1].asString();
                if (args[0].isDictionary()) {
                    auto dictPtr = args[0].asDictionaryPtr();
                    dictPtr->modifyMap([&](auto& m) { m[prop] = args[2]; });
                    return args[0];
                }
                if (args[0].isInstance()) {
                    args[0].asInstance()->setProperty(prop, args[2]);
                    return args[0];
                }
                interp.runtimeError("setattr() expects dictionary or instance object", 0, ""); return Value();
            }));

    interp.defineGlobal("hasattr", Value::makeNativeFunction("hasattr", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[1].isString()) { interp.runtimeError("hasattr() expects property name as string", 0, ""); return Value(); }
                std::string prop = args[1].asString();
                if (args[0].isDictionary()) {
                    // Was comparing iterators from two DIFFERENT temporary map copies
                    // (undefined behaviour, and 2x O(n)). One locked O(1) lookup.
                    return Value(args[0].asDictionaryPtr()->has(prop));
                }
                if (args[0].isInstance()) {
                    return Value(args[0].asInstance()->hasProperty(prop));
                }
                interp.runtimeError("hasattr() expects dictionary or instance object", 0, ""); return Value();
            }));

    

}
