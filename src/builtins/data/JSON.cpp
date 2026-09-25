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

void registerJSONBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("parse_json", Value::makeNativeFunction("parse_json", 1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                if (!args[0].isString()) { interp.throwException("TypeError", "parse_json() expects string"); return Value(); }
                MiniJson::Value root; MiniJson::Reader reader;
                // Was runtimeError(...) -- bare string, so catch (e) { e.message }
                // failed (same bug class fixed elsewhere for RateLimitError,
                // AssertionError, awaitAll/awaitAny). Now that the parser itself
                // correctly rejects malformed input (see MiniJson.h) instead of
                // silently returning garbage, this path is reachable for any
                // ordinary malformed-JSON input, not just a rare edge case.
                if (!reader.parse(args[0].asString(), root)) { interp.throwException("ValueError", "Failed to parse JSON"); return Value(); }
                
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
                        else {
                            // Was std::to_string(d), which always pads to exactly
                            // 6 decimal places (3.14 -> "3.140000" in the emitted
                            // JSON) -- inconsistent with how the same number
                            // prints via str()/out elsewhere. Match those instead.
                            std::ostringstream oss;
                            oss << d;
                            numVal.stringVal = oss.str();
                        }
                        return numVal;
                    }
                    else if (v.isBool()) {
                        MiniJson::Value boolVal(MiniJson::BOOLEAN);
                        boolVal.stringVal = v.asBool() ? "true" : "false";
                        return boolVal;
                    }
                    // Was MiniJson::Value("null") -- that constructor overload
                    // builds a STRING-typed value holding the text "null", not
                    // MiniJson's ALL_NULL type, so to_json(nil) emitted the JSON
                    // string "null" (quoted) instead of the JSON literal null.
                    // MiniJson::Value() (no args) is the one that actually
                    // defaults to ALL_NULL.
                    return MiniJson::Value();
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

}
