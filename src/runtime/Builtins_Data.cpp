#include "../Builtins.h"
#include "../RuntimeContext.h"
#include "../MiniJson.h"

#include <string>
#include <vector>
#include <algorithm>
#include <sstream>

void registerDataBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("len", Value::makeNativeFunction("len", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isNil()) return Value(0LL);
            if (args[0].isString()) return Value(static_cast<long long>(args[0].asString().length()));
            if (args[0].isArray()) return Value(static_cast<long long>(args[0].asArray().size()));
            if (args[0].isDictionary()) return Value(static_cast<long long>(args[0].asDictionary().map.size()));
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

    interp.defineGlobal("str", Value::makeNativeFunction("str", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            return Value(interp.stringify(args[0]));
        }));

    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isNumber()) return args[0];
            if (args[0].isInteger()) return args[0];
            if (args[0].isString()) {
                try {
                    std::string s = args[0].asString();
                    if (s.find('.') == std::string::npos && s.find('e') == std::string::npos && s.find('E') == std::string::npos) {
                        return Value(std::stoll(s));
                    }
                    return Value(std::stod(s));
                } 
                catch (...) { interp.runtimeError("Cannot convert '" + args[0].asString() + "' to number", 0, ""); return Value(); }
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

    interp.defineGlobal("keys", Value::makeNativeFunction("keys", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("keys() expects dictionary as first argument", 0, ""); return Value(); }
            auto dictPtr = args[0].asDictionaryPtr();
            std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
            std::vector<Value> keys;
            for (const auto& kv : dictPtr->map) keys.push_back(Value(kv.first));
            return Value::makeArray(keys);
        }));

    interp.defineGlobal("values", Value::makeNativeFunction("values", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("values() expects dictionary as first argument", 0, ""); return Value(); }
            auto dictPtr = args[0].asDictionaryPtr();
            std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
            std::vector<Value> vals;
            for (const auto& kv : dictPtr->map) vals.push_back(kv.second);
            return Value::makeArray(vals);
        }));

    interp.defineGlobal("has_key", Value::makeNativeFunction("has_key", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("has_key() expects dictionary as first argument", 0, ""); return Value(); }
            std::string key = args[1].toString();
            auto dictPtr = args[0].asDictionaryPtr();
            std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
            return Value(dictPtr->map.find(key) != dictPtr->map.end());
        }));

    interp.defineGlobal("remove", Value::makeNativeFunction("remove", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("remove() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("remove() expects number index as second argument", 0, ""); return Value(); }
            auto& arr = *args[0].asArrayPtr();
            int index = static_cast<int>(args[1].asNumber());
            if (index < 0 || index >= static_cast<int>(arr.size())) { interp.runtimeError("remove() index out of bounds", 0, ""); return Value(); }
            Value removed = arr[index];
            arr.erase(arr.begin() + index);
            return removed;
        }));

    interp.defineGlobal("dictRemove", Value::makeNativeFunction("dictRemove", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("dictRemove() expects dictionary", 0, ""); return Value(); }
            std::string key = args[1].toString();
            auto dictPtr = args[0].asDictionaryPtr();
            std::unique_lock<std::shared_mutex> lk(dictPtr->map_mutex);
            dictPtr->map.erase(key);
            return args[0];
        }));

    interp.defineGlobal("insert", Value::makeNativeFunction("insert", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("insert() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("insert() expects number index as second argument", 0, ""); return Value(); }
            auto& arr = *args[0].asArrayPtr();
            int index = static_cast<int>(args[1].asNumber());
            if (index < 0 || index > static_cast<int>(arr.size())) { interp.runtimeError("insert() index out of bounds", 0, ""); return Value(); }
            arr.insert(arr.begin() + index, args[2]);
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
                return Value::makeArray(std::vector<Value>(arr.begin() + start, arr.begin() + end));
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

    interp.defineGlobal("map", Value::makeNativeFunction("map", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("map() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isCallable()) { interp.runtimeError("map() expects function as second argument", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            for (const auto& elem : arr) result.push_back(interp.callFunction(args[1], {elem}, 0, "native"));
            return Value::makeArray(result);
        }));

    interp.defineGlobal("filter", Value::makeNativeFunction("filter", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("filter() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isCallable()) { interp.runtimeError("filter() expects function as second argument", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            for (const auto& elem : arr) {
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
            for (const auto& elem : arr) acc = interp.callFunction(args[1], {acc, elem}, 0, "native");
            return acc;
        }));

    interp.defineGlobal("forEach", Value::makeNativeFunction("forEach", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("forEach() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isCallable()) { interp.runtimeError("forEach() expects function as second argument", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            for (const auto& elem : arr) interp.callFunction(args[1], {elem}, 0, "native");
            return Value();
        }));

    interp.defineGlobal("find", Value::makeNativeFunction("find", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("find() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isCallable()) { interp.runtimeError("find() expects function as second argument", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            for (const auto& elem : arr) {
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
            for (const auto& elem : arr) {
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
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0, "native");
                if (test.isTruthy()) return Value(true);
            }
            return Value(false);
        }));

    interp.defineGlobal("contains", Value::makeNativeFunction("contains", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                if (!args[1].isString()) { interp.runtimeError("contains() with string expects string to search for", 0, ""); return Value(); }
                return Value(args[0].asString().find(args[1].asString()) != std::string::npos);
            }
            if (args[0].isArray()) {
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr) {
                    if (elem.equals(args[1])) return Value(true);
                }
                return Value(false);
            }
            if (args[0].isDictionary()) {
                std::string key = args[1].toString();
                auto dictPtr = args[0].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                return Value(dictPtr->map.find(key) != dictPtr->map.end());
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
                    if (val < 0) return Value(-1.0); // Safe if negative
                    startPos = static_cast<size_t>(val);
                }
                if (startPos >= args[0].asString().length() && args[0].asString().length() > 0) return Value(-1.0);
                
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
                    if (val < 0) return Value(-1.0);
                    startPos = static_cast<size_t>(val);
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
                std::string s = args[0].asString();
                std::reverse(s.begin(), s.end());
                return Value(s);
            }
            if (args[0].isArray()) {
                auto arr = args[0].asArray();
                std::reverse(arr.begin(), arr.end());
                return Value::makeArrayCopy(arr);
            }
            interp.runtimeError("reverse() expects string or array", 0, ""); return Value();
         }));

    interp.defineGlobal("sort", Value::makeNativeFunction("sort", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("sort() expects array", 0, ""); return Value(); }
            auto arr = args[0].asArray();
            std::sort(arr.begin(), arr.end(), [](const Value& a, const Value& b) {
                if (a.isNumber() && b.isNumber()) return a.asNumber() < b.asNumber();
                return a.toString() < b.toString();
            });
            return Value::makeArrayCopy(arr);
        }));

    interp.defineGlobal("parse_json", Value::makeNativeFunction("parse_json", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("parse_json() expects string", 0, ""); return Value(); }
            MiniJson::Value root; MiniJson::Reader reader;
            if (!reader.parse(args[0].asString(), root)) { interp.runtimeError("Failed to parse JSON", 0, ""); return Value(); }
            
            std::function<Value(const MiniJson::Value&)> convert;
            convert = [&](const MiniJson::Value& mv) -> Value {
                if (mv.type == MiniJson::OBJECT) {
                    Value dv = Value::makeDictionary(); auto& map = dv.asDictionary().map;
                    for (const auto& name : mv.getMemberNames()) map[name] = convert(mv[name]);
                    return dv;
                } else if (mv.type == MiniJson::ARRAY) {
                    std::vector<Value> av; for (const auto& item : mv.items) av.push_back(convert(item));
                    return Value::makeArray(av);
                } else {
                    std::string s = mv.asString();
                    if (s == "true") return Value(true); if (s == "false") return Value(false); if (s == "null") return Value();
                    if (!s.empty() && (isdigit(s[0]) || s[0] == '-' || s[0] == '.')) {
                        try { size_t pos; double d = std::stod(s, &pos); if (pos == s.length()) return Value(d); } catch (...) {}
                    }
                    return Value(s);
                }
            };
            return convert(root);
        }));

    interp.defineGlobal("to_json", Value::makeNativeFunction("to_json", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::function<MiniJson::Value(const Value&)> convert;
            convert = [&](const Value& v) -> MiniJson::Value {
                if (v.isDictionary()) {
                    MiniJson::Value mv(MiniJson::OBJECT);
                    auto dictPtr = v.asDictionaryPtr();
                    std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                    for (const auto& kv : dictPtr->map) mv[kv.first] = convert(kv.second);
                    return mv;
                } else if (v.isArray()) {
                    MiniJson::Value mv(MiniJson::ARRAY);
                    for (const auto& item : v.asArray()) mv.append(convert(item));
                    return mv;
                } else if (v.isString()) return MiniJson::Value(v.asString());
                else if (v.isNumber()) {
                    double d = v.asNumber();
                    MiniJson::Value numVal(MiniJson::NUMBER);
                    if (d == (int)d) numVal.stringVal = std::to_string((int)d);
                    else numVal.stringVal = std::to_string(d);
                    return numVal;
                }
                else if (v.isBool()) {
                    MiniJson::Value boolVal(MiniJson::BOOLEAN);
                    boolVal.stringVal = v.asBool() ? "true" : "false";
                    return boolVal;
                }
                return MiniJson::Value("null");
            };
            MiniJson::Value root = convert(args[0]);
            std::stringstream ss; MiniJson::StreamWriter writer;
            writer.write(root, &ss);
            return Value(ss.str());
        }));

    interp.defineGlobal("getattr", Value::makeNativeFunction("getattr", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[1].isString()) { interp.runtimeError("getattr() expects property name as string", 0, ""); return Value(); }
            std::string prop = args[1].asString();
            if (args[0].isDictionary()) {
                auto dictPtr = args[0].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                auto it = dictPtr->map.find(prop);
                if (it != dictPtr->map.end()) return it->second;
                return Value();
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
                std::unique_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                dictPtr->map[prop] = args[2];
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
                auto dictPtr = args[0].asDictionaryPtr();
                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                return Value(dictPtr->map.find(prop) != dictPtr->map.end());
            }
            if (args[0].isInstance()) {
                return Value(args[0].asInstance()->hasProperty(prop));
            }
            interp.runtimeError("hasattr() expects dictionary or instance object", 0, ""); return Value();
        }));
}
