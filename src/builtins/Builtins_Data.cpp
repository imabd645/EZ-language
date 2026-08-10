#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Utf8.h"
#include "utils/MiniJson.h"

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
            std::vector<Value> keys;
            for (const auto& kv : dictPtr->getMapCopy()) keys.push_back(Value(kv.first));
            return Value::makeArray(keys);
        }));

    interp.defineGlobal("properties", Value::makeNativeFunction("properties", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isInstance()) { interp.runtimeError("properties() expects an instance object", 0, ""); return Value(); }
            auto instancePtr = args[0].asInstance();
            std::vector<Value> keys;
            for (const auto& kv : instancePtr->getPropertiesCopy()) keys.push_back(Value(kv.first));
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

    interp.defineGlobal("dictRemove", Value::makeNativeFunction("dictRemove", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("dictRemove() expects dictionary", 0, ""); return Value(); }
            std::string key = args[1].toString();
            auto dictPtr = args[0].asDictionaryPtr();
            dictPtr->erase(key);
            return args[0];
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

    interp.defineGlobal("parse_json", Value::makeNativeFunction("parse_json", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("parse_json() expects string", 0, ""); return Value(); }
            MiniJson::Value root; MiniJson::Reader reader;
            if (!reader.parse(args[0].asString(), root)) { interp.runtimeError("Failed to parse JSON", 0, ""); return Value(); }
            
            std::function<Value(const MiniJson::Value&)> convert;
            convert = [&](const MiniJson::Value& mv) -> Value {
                if (mv.type == MiniJson::OBJECT) {
                    Value dv = Value::makeDictionary(); 
                    dv.asDictionary().modifyMap([&](auto& m) {
                        for (const auto& name : mv.getMemberNames()) m[name] = convert(mv[name]);
                    });
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
            // Containers on the path from the root to the value being converted.
            // Without this a self-referential value recursed until the native
            // stack gave out and killed the process:
            //
            //     a = []
            //     push(a, a)
            //     to_json(a)        <- crash
            //
            // JSON has no way to express a cycle, so unlike toString() -- which
            // can print a `[...]` marker -- there is nothing sensible to emit.
            // It is reported as a catchable error instead. `depth` additionally
            // stops a very deeply nested (but acyclic) value from exhausting the
            // stack before it ever reaches a cycle.
            std::vector<const void*> path;
            bool failed = false;
            std::string failure;
            const size_t JSON_MAX_DEPTH = 512;

            std::function<MiniJson::Value(const Value&)> convert;
            convert = [&](const Value& v) -> MiniJson::Value {
                if (failed) return MiniJson::Value("null");

                const void* id = nullptr;
                if (v.isDictionary())  id = (const void*)v.asDictionaryPtr().get();
                else if (v.isArray())  id = (const void*)v.asArrayPtr().get();

                if (id) {
                    for (const void* seen : path) {
                        if (seen == id) {
                            failed = true;
                            failure = "to_json(): value contains a reference to itself, "
                                      "which JSON cannot represent";
                            return MiniJson::Value("null");
                        }
                    }
                    if (path.size() >= JSON_MAX_DEPTH) {
                        failed = true;
                        failure = "to_json(): value nested deeper than " +
                                  std::to_string(JSON_MAX_DEPTH) + " levels";
                        return MiniJson::Value("null");
                    }
                    path.push_back(id);
                }
                // Pops `id` on every return path below.
                struct PathPop {
                    std::vector<const void*>& p; bool active;
                    ~PathPop() { if (active) p.pop_back(); }
                } pop{path, id != nullptr};

                if (v.isDictionary()) {
                    MiniJson::Value mv(MiniJson::OBJECT);
                    auto dictPtr = v.asDictionaryPtr();
                    for (const auto& kv : dictPtr->getMapCopy()) mv[kv.first] = convert(kv.second);
                    return mv;
                } else if (v.isArray()) {
                    MiniJson::Value mv(MiniJson::ARRAY);
                    for (const auto& item : v.asArray().getElementsCopy()) mv.append(convert(item));
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
            if (failed) {
                interp.throwException("ValueError", failure, 0, "");
                return Value();
            }
            std::stringstream ss; MiniJson::StreamWriter writer;
            writer.write(root, &ss);
            return Value(ss.str());
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

    interp.defineGlobal("parse_csv", Value::makeNativeFunction("parse_csv", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("parse_csv() expects string", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            std::vector<std::vector<std::string>> rows;
            std::vector<std::string> currentRow;
            std::string currentField;
            bool inQuotes = false;
            
            for (size_t i = 0; i < str.length(); ++i) {
                char c = str[i];
                if (inQuotes) {
                    if (c == '"') {
                        if (i + 1 < str.length() && str[i + 1] == '"') {
                            currentField += '"';
                            i++;
                        } else {
                            inQuotes = false;
                        }
                    } else {
                        currentField += c;
                    }
                } else {
                    if (c == '"') {
                        inQuotes = true;
                    } else if (c == ',') {
                        currentRow.push_back(currentField);
                        currentField = "";
                    } else if (c == '\r') {
                        // ignore
                    } else if (c == '\n') {
                        currentRow.push_back(currentField);
                        rows.push_back(currentRow);
                        currentRow.clear();
                        currentField = "";
                    } else {
                        currentField += c;
                    }
                }
            }
            if (!currentField.empty() || !currentRow.empty()) {
                currentRow.push_back(currentField);
                rows.push_back(currentRow);
            }
            
            if (rows.empty()) return Value::makeArray(std::vector<Value>());
            
            std::vector<std::string> headers = rows[0];
            std::vector<Value> result;
            
            for (size_t i = 1; i < rows.size(); ++i) {
                Value dictVal = Value::makeDictionary();
                auto dict = dictVal.asDictionaryPtr();
                dict->modifyMap([&](auto& m) {
                    for (size_t j = 0; j < rows[i].size() && j < headers.size(); ++j) {
                        m[headers[j]] = Value(rows[i][j]);
                    }
                });
                result.push_back(dictVal);
            }
            return Value::makeArray(result);
        }));

    interp.defineGlobal("to_csv", Value::makeNativeFunction("to_csv", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("to_csv() expects an array of dictionaries", 0, ""); return Value(); }
            const auto& arr = args[0].asArray().getElementsCopy();
            if (arr.empty()) return Value("");
            
            std::vector<std::string> headers;
            if (arr[0].isDictionary()) {
                auto m = arr[0].asDictionaryPtr()->getMapCopy();
                for (auto& kv : m) headers.push_back(kv.first);
            } else {
                return Value("");
            }
            
            std::string res;
            auto escape = [](const std::string& s) {
                if (s.find(',') != std::string::npos || s.find('"') != std::string::npos || s.find('\n') != std::string::npos || s.find('\r') != std::string::npos) {
                    std::string esc = "\"";
                    for (char c : s) {
                        if (c == '"') esc += "\"\"";
                        else esc += c;
                    }
                    esc += "\"";
                    return esc;
                }
                return s;
            };
            
            for (size_t i = 0; i < headers.size(); i++) {
                res += escape(headers[i]);
                if (i < headers.size() - 1) res += ",";
            }
            res += "\n";
            
            for (const auto& rowVal : arr) {
                if (rowVal.isDictionary()) {
                    auto dictPtr = rowVal.asDictionaryPtr();
                    for (size_t i = 0; i < headers.size(); i++) {
                        Value v = dictPtr->get(headers[i]);
                        std::string sVal = v.isString() ? v.asString() : (v.isNil() ? "" : v.toString());
                        res += escape(sVal);
                        if (i < headers.size() - 1) res += ",";
                    }
                    res += "\n";
                }
            }
            return Value(res);
        }));
}
