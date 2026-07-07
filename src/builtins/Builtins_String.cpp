#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <regex>

void registerStringBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("substr", Value::makeNativeFunction("substr", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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

    // bytesToString(arr) — O(n) conversion of a byte array to a string.
    // Each element must be an integer in range [0, 255].
    // This is the fast path for Base64.decode(), _toHex(), HMAC._compute(), etc.
    interp.defineGlobal("bytesToString", Value::makeNativeFunction("bytesToString", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) { interp.runtimeError("bytesToString() expects an array", 0, ""); return Value(); }
            const auto& arr = args[0].asArray();
            std::string result;
            result.reserve(arr.size());
            for (const Value& v : arr) {
                if (!v.isNumber()) { interp.runtimeError("bytesToString() expects array of integers (0-255)", 0, ""); return Value(); }
                long long b = v.isInteger() ? v.asInteger() : (long long)v.asFloat();
                result += static_cast<char>(b & 0xFF);
            }
            return Value(result);
        }));

    
    interp.defineGlobal("upper", Value::makeNativeFunction("upper", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("upper() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value(s);
        }));

    interp.defineGlobal("toUpper", Value::makeNativeFunction("toUpper", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toUpper() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    interp.defineGlobal("lower", Value::makeNativeFunction("lower", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("lower() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value(s);
        }));

    interp.defineGlobal("toLower", Value::makeNativeFunction("toLower", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toLower() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    interp.defineGlobal("trim", Value::makeNativeFunction("trim", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("trim() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
            return Value(s);
        }));

    interp.defineGlobal("replace", Value::makeNativeFunction("replace", 3,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("startsWith() expects two strings", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            const std::string& prefix = args[1].asString();
            if (prefix.length() > str.length()) return Value(false);
            return Value(str.compare(0, prefix.length(), prefix) == 0);
        }));

    interp.defineGlobal("endsWith", Value::makeNativeFunction("endsWith", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) { interp.runtimeError("endsWith() expects two strings", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            const std::string& suffix = args[1].asString();
            if (suffix.length() > str.length()) return Value(false);
            return Value(str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
        }));

    interp.defineGlobal("ord", Value::makeNativeFunction("ord", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("ord() expects string", 0, ""); return Value(); }
            std::string s = args[0].asString();
            if (s.empty()) return Value(0LL);
            return Value((long long)(unsigned char)s[0]);
        }));

    interp.defineGlobal("chr", Value::makeNativeFunction("chr", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("chr() expects a number", 0, ""); return Value(); }
            char c = (char)args[0].asNumber();
            return Value(std::string(1, c));
        }));

    interp.defineGlobal("substring", Value::makeNativeFunction("substring", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
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

    interp.defineGlobal("hex_to_bytes", Value::makeNativeFunction("hex_to_bytes", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("hex_to_bytes() expects a string", 0, ""); return Value(); }
            std::string hex = args[0].asString();
            std::vector<Value> bytes;
            for (size_t i = 0; i + 1 < hex.length(); i += 2) {
                int hi = 0, lo = 0;
                char c1 = std::tolower(hex[i]), c2 = std::tolower(hex[i + 1]);
                if (c1 >= '0' && c1 <= '9') hi = c1 - '0'; else if (c1 >= 'a' && c1 <= 'f') hi = c1 - 'a' + 10;
                if (c2 >= '0' && c2 <= '9') lo = c2 - '0'; else if (c2 >= 'a' && c2 <= 'f') lo = c2 - 'a' + 10;
                bytes.push_back(Value((double)(hi * 16 + lo)));
            }
            return Value::makeArray(bytes);
        }));

    static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    
    interp.defineGlobal("b64url_encode", Value::makeNativeFunction("b64url_encode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() && !args[0].isArray()) { interp.runtimeError("b64url_encode() expects string or array", 0, ""); return Value(); }
            std::string in;
            if (args[0].isString()) in = args[0].asString();
            else {
                for (auto& v : args[0].asArray()) in += (char)v.asNumber();
            }
            std::string out;
            int val = 0, valb = -6;
            for (unsigned char c : in) {
                val = (val << 8) + c;
                valb += 8;
                while (valb >= 0) {
                    out.push_back(b64_chars[(val >> valb) & 0x3F]);
                    valb -= 6;
                }
            }
            if (valb > -6) out.push_back(b64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
            return Value(out);
        }));

    interp.defineGlobal("b64url_decode", Value::makeNativeFunction("b64url_decode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("b64url_decode() expects string", 0, ""); return Value(); }
            std::string in = args[0].asString();
            std::string out;
            std::vector<int> T(256, -1);
            for (int i = 0; i < 64; i++) T[b64_chars[i]] = i;
            int val = 0, valb = -8;
            for (unsigned char c : in) {
                if (T[c] == -1) break;
                val = (val << 6) + T[c];
                valb += 6;
                if (valb >= 0) {
                    out.push_back(char((val >> valb) & 0xFF));
                    valb -= 8;
                }
            }
            return Value(out);
        }));
}
