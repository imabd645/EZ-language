#include "../Builtins.h"
#include "../Interpreter.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>

void registerStringBuiltins(Interpreter& interp) {
    interp.defineGlobal("substr", Value::makeNativeFunction("substr", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("substr() expects string as first argument", 0, ""); return Value(); }
            if (!args[1].isNumber() || !args[2].isNumber()) { interp.runtimeError("substr() expects numbers for start and length", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            int start = static_cast<int>(args[1].asNumber());
            int len = static_cast<int>(args[2].asNumber());
            
            if (start < 0) start = 0;
            if (start >= static_cast<int>(str.length())) return Value("");
            if (len < 0) len = 0;
            
            return Value(str.substr(start, len));
        }));

    interp.defineGlobal("split", Value::makeNativeFunction("split", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("split() expects two strings", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            const std::string& delim = args[1].asString();
            std::vector<Value> result;
            
            if (delim.empty()) {
                for (char c : str) { result.push_back(Value(std::string(1, c))); }
            } else {
                size_t start = 0;
                size_t end = str.find(delim);
                while (end != std::string::npos) {
                    result.push_back(Value(str.substr(start, end - start)));
                    start = end + delim.length();
                    end = str.find(delim, start);
                }
                result.push_back(Value(str.substr(start)));
            }
            return Value::makeArray(result);
        }));

    interp.defineGlobal("join", Value::makeNativeFunction("join", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("join() expects array as first argument", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("join() expects string as delimiter", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            const std::string& delim = args[1].asString();
            std::string result;
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += delim;
                result += arr[i].toString();
            }
            return Value(result);
        }));
    
    interp.defineGlobal("upper", Value::makeNativeFunction("upper", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("upper() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value(s);
        }));

    interp.defineGlobal("toUpper", Value::makeNativeFunction("toUpper", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toUpper() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    interp.defineGlobal("lower", Value::makeNativeFunction("lower", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("lower() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value(s);
        }));

    interp.defineGlobal("toLower", Value::makeNativeFunction("toLower", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toLower() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    interp.defineGlobal("trim", Value::makeNativeFunction("trim", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("trim() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
            return Value(s);
        }));

    interp.defineGlobal("replace", Value::makeNativeFunction("replace", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString() || !args[2].isString()) { interp.runtimeError("replace() expects three strings", 0, ""); return Value(); }
            std::string s = args[0].asString();
            const std::string& from = args[1].asString();
            const std::string& to = args[2].asString();
            if (from.empty()) return Value(s);
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.length(), to);
                pos += to.length();
            }
            return Value(s);
        }));

    interp.defineGlobal("startsWith", Value::makeNativeFunction("startsWith", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("startsWith() expects two strings", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            const std::string& prefix = args[1].asString();
            if (prefix.length() > str.length()) return Value(false);
            return Value(str.compare(0, prefix.length(), prefix) == 0);
        }));

    interp.defineGlobal("endsWith", Value::makeNativeFunction("endsWith", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("endsWith() expects two strings", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            const std::string& suffix = args[1].asString();
            if (suffix.length() > str.length()) return Value(false);
            return Value(str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
        }));

    interp.defineGlobal("ord", Value::makeNativeFunction("ord", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("ord() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            if (s.empty()) return Value(0.0);
            return Value((double)(unsigned char)s[0]);
        }));

    interp.defineGlobal("chr", Value::makeNativeFunction("chr", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("chr() expects number", 0, ""); return Value(); }
            char c = (char)(int)args[0].asNumber();
            return Value(std::string(1, c));
        }));

    interp.defineGlobal("substring", Value::makeNativeFunction("substring", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) { interp.runtimeError("substring() expects 2 or 3 arguments", 0, ""); return Value(); }
            if (!args[0].isString()) { interp.runtimeError("substring() first arg must be string", 0, ""); return Value(); }
            if (!args[1].isNumber()) { interp.runtimeError("substring() start must be number", 0, ""); return Value(); }
            
            std::string s = args[0].asString();
            int start = (int)args[1].asNumber();
            int len = (args.size() == 3 && args[2].isNumber()) ? (int)args[2].asNumber() : (int)s.length() - start;
            
            if (start < 0) start = 0;
            if (start > (int)s.length()) return Value("");
            if (len < 0) len = 0;
            
            return Value(s.substr(start, len));
        }));

    // --- Regex Engine ---
    
    interp.defineGlobal("reMatch", Value::makeNativeFunction("reMatch", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("reMatch() expects two strings (text, pattern)", 0, ""); return Value(); }
            try {
                std::regex re(args[1].asString());
                return Value(std::regex_match(args[0].asString(), re));
            } catch (const std::regex_error& e) {
                interp.runtimeError(std::string("Regex Error: ") + e.what(), 0, "");
                return Value(false);
            }
        }));

    interp.defineGlobal("reSearch", Value::makeNativeFunction("reSearch", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("reSearch() expects two strings (text, pattern)", 0, ""); return Value(); }
            try {
                std::string text = args[0].asString();
                std::regex re(args[1].asString());
                std::smatch matches;
                std::vector<Value> results;
                if (std::regex_search(text, matches, re)) {
                    for (size_t i = 0; i < matches.size(); i++) {
                        results.push_back(Value(matches[i].str()));
                    }
                }
                return Value::makeArray(results);
            } catch (const std::regex_error& e) {
                interp.runtimeError(std::string("Regex Error: ") + e.what(), 0, "");
                return Value::makeArray({});
            }
        }));

    interp.defineGlobal("reReplace", Value::makeNativeFunction("reReplace", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString() || !args[2].isString()) { 
                interp.runtimeError("reReplace() expects three strings (text, pattern, replacement)", 0, ""); return Value(); 
            }
            try {
                std::regex re(args[1].asString());
                std::string result = std::regex_replace(args[0].asString(), re, args[2].asString());
                return Value(result);
            } catch (const std::regex_error& e) {
                interp.runtimeError(std::string("Regex Error: ") + e.what(), 0, "");
                return Value(args[0]);
            }
        }));
}
