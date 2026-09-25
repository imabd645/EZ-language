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

void registerArrayBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("len", Value::makeNativeFunction("len", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args[0].isNil()) return Value(0LL);
                if (args[0].isString()) return Value(static_cast<long long>(args[0].asString().length()));
                if (args[0].isArray()) return Value(static_cast<long long>(args[0].asArray().size()));
                if (args[0].isDictionary()) return Value(static_cast<long long>(args[0].asDictionary().size()));
                if (args[0].isBuffer()) return Value(static_cast<long long>(args[0].asBuffer().size()));
                interp.runtimeError("len() expects string or array", 0, ""); return Value();
             }));

    interp.defineGlobal("push", Value::makeNativeFunction("push", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("push() expects array as first argument", 0, ""); return Value(); }
                auto arr = args[0].asArrayPtr();
                arr->push_back(args[1]);
                return Value(arr);
            }));

    interp.defineGlobal("pop", Value::makeNativeFunction("pop", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("pop() expects array", 0, ""); return Value(); }
                auto& arr = *args[0].asArrayPtr();
                if (arr.empty()) { interp.runtimeError("pop() on empty array", 0, ""); return Value(); }
                Value last = arr.back();
                arr.pop_back();
                return last;
            }));

    interp.defineGlobal("remove", Value::makeNativeFunction("remove", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("remove() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isNumber()) { interp.runtimeError("remove() expects number index as second argument", 0, ""); return Value(); }
                auto& arr = *args[0].asArrayPtr();
                int index = static_cast<int>(args[1].asNumber());
                if (index < 0 || index >= static_cast<int>(arr.size())) { interp.runtimeError("remove() index out of bounds", 0, ""); return Value(); }
                Value removed = arr[index];
                arr.erase(index);
                return removed;
            }));

    interp.defineGlobal("insert", Value::makeNativeFunction("insert", 3,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("insert() expects array as first argument", 0, ""); return Value(); }
                if (!args[1].isNumber()) { interp.runtimeError("insert() expects number index as second argument", 0, ""); return Value(); }
                auto& arr = *args[0].asArrayPtr();
                int index = static_cast<int>(args[1].asNumber());
                if (index < 0 || index > static_cast<int>(arr.size())) { interp.runtimeError("insert() index out of bounds", 0, ""); return Value(); }
                arr.insert(index, args[2]);
                return args[0];
            }));

    interp.defineGlobal("slice", Value::makeNativeFunction("slice", 3,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[1].isNumber() || !args[2].isNumber()) { interp.runtimeError("slice() expects numbers for start and end", 0, ""); return Value(); }
                int start = static_cast<int>(args[1].asNumber());
                int end = static_cast<int>(args[2].asNumber());
                
                if (args[0].isString()) {
                    const std::string& s = args[0].asString();
                    int len = static_cast<int>(s.length());
                    if (start < 0) start = std::max(0, len + start);
                    if (end < 0) end = std::max(0, len + end);
                    if (start >= len) return Value("");
                    if (end > len) end = len;
                    if (start >= end) return Value("");
                    return Value(s.substr(start, end - start));
                }
                if (args[0].isArray()) {
                    const auto& arr = args[0].asArray();
                    int len = static_cast<int>(arr.size());
                    if (start < 0) start = std::max(0, len + start);
                    if (end < 0) end = std::max(0, len + end);
                    if (start >= len) return Value::makeArray({});
                    if (end > len) end = len;
                    if (start >= end) return Value::makeArray({});
                    auto copy = arr.getElementsCopy(); return Value::makeArray(std::vector<Value>(copy.begin() + start, copy.begin() + end));
                }
                interp.runtimeError("slice() expects string or array", 0, ""); return Value();
             }));

    interp.defineGlobal("range", Value::makeNativeFunction("range", -1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args.empty() || args.size() > 2) { interp.runtimeError("range() expects 1 or 2 arguments", 0, ""); return Value(); }
                int start = 0, end = 0;
                if (args.size() == 1) {
                    if (!args[0].isNumber()) { interp.runtimeError("range() expects number", 0, ""); return Value(); }
                    end = static_cast<int>(args[0].asNumber());
                } else {
                    if (!args[0].isNumber() || !args[1].isNumber()) { interp.runtimeError("range() expects numbers", 0, ""); return Value(); }
                    start = static_cast<int>(args[0].asNumber());
                    end = static_cast<int>(args[1].asNumber());
                }
                std::vector<Value> result;
                for (int i = start; i < end; i++) {
                    result.push_back(Value(static_cast<double>(i)));
                }
                return Value::makeArray(result);
            }));

    interp.defineGlobal("contains", Value::makeNativeFunction("contains", 2,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args[0].isString()) {
                    if (!args[1].isString()) { interp.runtimeError("contains() with string expects string to search for", 0, ""); return Value(); }
                    return Value(args[0].asString().find(args[1].asString()) != std::string::npos);
                }
                if (args[0].isArray()) {
                    const auto& arr = args[0].asArray();
                    for (const auto& elem : arr.getElementsCopy()) {
                        if (elem.equals(args[1])) return Value(true);
                    }
                    return Value(false);
                }
                if (args[0].isDictionary()) {
                    std::string key = args[1].toString();
                    auto dictPtr = args[0].asDictionaryPtr();
                    return Value(dictPtr->has(key));
                }
                interp.runtimeError("contains() expects string, array, or dictionary", 0, ""); return Value();
             }));

    interp.defineGlobal("indexOf", Value::makeNativeFunction("indexOf", -1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args.size() < 2 || args.size() > 3) {
                    interp.runtimeError("indexOf() expects 2 or 3 arguments", 0, ""); return Value();
                }
                if (args[0].isString()) {
                    if (!args[1].isString()) { interp.runtimeError("indexOf() with string expects string to search for", 0, ""); return Value(); }
                    size_t startPos = 0;
                    if (args.size() == 3) {
                        if (!args[2].isNumber()) { interp.runtimeError("indexOf() start position must be a number", 0, ""); return Value(); }
                        double val = args[2].asNumber();
                        int len = static_cast<int>(args[0].asString().length());
                        int start = static_cast<int>(val);
                        if (start < 0) start = std::max(0, len + start);
                        startPos = static_cast<size_t>(start);
                    }
    
                    size_t pos = args[0].asString().find(args[1].asString(), startPos);
                    if (pos == std::string::npos) return Value(-1.0);
                    return Value(static_cast<double>(pos));
                }
                if (args[0].isArray()) {
                    const auto& arr = args[0].asArray();
                    size_t startPos = 0;
                    if (args.size() == 3) {
                        if (!args[2].isNumber()) { interp.runtimeError("indexOf() start position must be a number", 0, ""); return Value(); }
                        double val = args[2].asNumber();
                        int len = static_cast<int>(arr.size());
                        int start = static_cast<int>(val);
                        if (start < 0) start = std::max(0, len + start);
                        startPos = static_cast<size_t>(start);
                    }
                    for (size_t i = startPos; i < arr.size(); i++) {
                        if (arr[i].equals(args[1])) return Value(static_cast<double>(i));
                    }
                    return Value(-1.0);
                }
                interp.runtimeError("indexOf() expects string or array", 0, ""); return Value();
             }));

    interp.defineGlobal("reverse", Value::makeNativeFunction("reverse", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (args[0].isString()) {
                    // Reverse characters, not bytes. std::reverse on the raw bytes
                    // turned "café" into the byte sequence 169 195 102 97 99 -- a
                    // valid string in, an invalid one out, because it flipped the
                    // two bytes of é against each other.
                    return Value(ez_utf8::reverseChars(args[0].asString()));
                }
                if (args[0].isArray()) {
                    auto arr = args[0].asArray().getElementsCopy(); std::reverse(arr.begin(), arr.end()); return Value::makeArray(arr);
                }
                interp.runtimeError("reverse() expects string or array", 0, ""); return Value();
             }));

    interp.defineGlobal("sort", Value::makeNativeFunction("sort", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isArray()) { interp.runtimeError("sort() expects array", 0, ""); return Value(); }
                auto arr = args[0].asArray().getElementsCopy(); std::sort(arr.begin(), arr.end(), [](const Value& a, const Value& b) {
                    if (a.isNumber() && b.isNumber()) return a.asNumber() < b.asNumber();
                    return a.toString() < b.toString();
                });
                return Value::makeArrayCopy(arr);
            }));

}
