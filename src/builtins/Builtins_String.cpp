#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Utf8.h"

#include <string>
#include <vector>
#include <algorithm>
#include <cctype>
#include <cstring>
#include <regex>

// ── Regex helpers ─────────────────────────────────────────────────────────────
// Shared by the re_* builtins below.

// Compiles `pattern` under a flag string ("i" case-insensitive, "m" multiline).
// Returns false having thrown RegexError if the pattern does not compile, so a
// typo surfaces as an error rather than as "no match".
static bool buildRegex(RuntimeContext& interp, const std::string& pattern,
                       const std::string& flags, std::regex& out) {
    auto opts = std::regex_constants::ECMAScript;
    for (char f : flags) {
        switch (f) {
            case 'i': opts |= std::regex_constants::icase; break;
            case 'm': opts |= std::regex_constants::multiline; break;
            case ' ': break;
            default:
                interp.throwException("RegexError",
                    std::string("Unknown regex flag '") + f + "'. Supported: 'i' (ignore case), "
                    "'m' (multiline).", 0, "");
                return false;
        }
    }
    try {
        out.assign(pattern, opts);
    } catch (const std::regex_error& e) {
        interp.throwException("RegexError",
            "Invalid pattern '" + pattern + "': " + e.what(), 0, "");
        return false;
    }
    return true;
}

// Pulls (text, pattern, flags) off the argument list, with flags optional.
static bool regexArgs(RuntimeContext& interp, const std::vector<Value>& args,
                      const char* fname, std::string& text, std::string& pattern,
                      std::string& flags, size_t required) {
    if (args.size() < required || !args[0].isString() || !args[1].isString()) {
        interp.throwException("TypeError",
            std::string(fname) + "() expects (text, pattern[, flags])", 0, "");
        return false;
    }
    text = args[0].asString();
    pattern = args[1].asString();
    flags = (args.size() >= 3 && args[2].isString()) ? args[2].asString() : "";
    return true;
}

// Builds {"text", "start", "end", "groups"} for one match. `base` is the offset
// the search started from, so positions are absolute in the original string.
// An unmatched optional group is nil rather than "", which is the only way to
// tell `(a)?` that matched empty from one that did not participate.
static Value makeMatchValue(const std::smatch& m, size_t base) {
    Value dict = Value::makeDictionary();
    size_t start = base + static_cast<size_t>(m.position(0));
    dict.asDictionary().set("text", Value(m.str(0)));
    dict.asDictionary().set("start", Value(static_cast<long long>(start)));
    dict.asDictionary().set("end", Value(static_cast<long long>(start + m.length(0))));

    std::vector<Value> groups;
    for (size_t i = 1; i < m.size(); ++i) {
        groups.push_back(m[i].matched ? Value(m[i].str()) : Value());
    }
    dict.asDictionary().set("groups", Value::makeArray(groups));
    return dict;
}

void registerStringBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("urlEncode", Value::makeNativeFunction("urlEncode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("urlEncode() expects a string", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            std::string result;
            const char* hex = "0123456789ABCDEF";
            for (unsigned char c : str) {
                if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
                    result += c;
                } else if (c == ' ') {
                    result += '+';
                } else {
                    result += '%';
                    result += hex[c >> 4];
                    result += hex[c & 15];
                }
            }
            return Value(result);
        }));

    interp.defineGlobal("urlDecode", Value::makeNativeFunction("urlDecode", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("urlDecode() expects a string", 0, ""); return Value(); }
            const std::string& str = args[0].asString();
            std::string result;
            for (size_t i = 0; i < str.length(); ++i) {
                if (str[i] == '+') {
                    result += ' ';
                } else if (str[i] == '%' && i + 2 < str.length()) {
                    auto fromHex = [](char c) -> int {
                        if (c >= '0' && c <= '9') return c - '0';
                        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
                        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
                        return 0;
                    };
                    int val = (fromHex(str[i+1]) << 4) | fromHex(str[i+2]);
                    result += static_cast<char>(val);
                    i += 2;
                } else {
                    result += str[i];
                }
            }
            return Value(result);
        }));

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
                size_t i = 0;
                while (i < str.size()) {
                    size_t start = i;
                    size_t n = ez_utf8::seqLen((unsigned char)str[i]);
                    if (n == 1 || !ez_utf8::validAt(str, i, n)) n = 1;
                    result.push_back(Value(str.substr(start, n)));
                    i = start + n;
                }
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
            for (const Value& v : arr.getElementsCopy()) {
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
            // The Unicode code point, not the first byte. Returning the lead
            // byte made ord("é") 195 -- a value that is not the character and
            // that chr() could not turn back into it. ASCII is unaffected.
            size_t i = 0;
            return Value((long long)ez_utf8::decode(s, i));
        }));

    interp.defineGlobal("chr", Value::makeNativeFunction("chr", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("chr() expects a number", 0, ""); return Value(); }
            double raw = args[0].asNumber();
            if (raw < 0 || raw > 1114111.0) {
                interp.runtimeError("chr() expects a code point between 0 and 1114111", 0, "");
                return Value();
            }
            // Encode as UTF-8 rather than writing the low byte. chr(233) used to
            // produce the single byte 233, which is not valid UTF-8 on its own,
            // so any non-ASCII code point yielded a broken string.
            std::string out;
            if (!ez_utf8::encode((uint32_t)raw, out)) {
                interp.runtimeError("chr() cannot encode code point " +
                                    std::to_string((long long)raw) +
                                    " (surrogate halves are not characters)", 0, "");
                return Value();
            }
            return Value(out);
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

    // ── Regex, with positions ──────────────────────────────────────────────
    // reSearch() above returns only the matched substrings, which is not enough
    // to walk a string: locating each match by searching for its own text finds
    // the first LITERAL occurrence instead of the actual match, and a zero-width
    // match never advances. These report offsets so iteration is exact.
    //
    // `flags` is a string: "i" case-insensitive, "m" multiline (^/$ match at
    // line breaks). std::regex is ECMAScript, which has no dotall or named
    // groups, so neither is offered rather than faked.
    //
    // An invalid pattern throws RegexError -- catchable, and never mistaken for
    // "no match".

    interp.defineGlobal("re_escape", Value::makeNativeFunction("re_escape", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.throwException("TypeError", "re_escape() expects a string", 0, "");
                return Value();
            }
            const std::string& in = args[0].asString();
            std::string out;
            out.reserve(in.size() * 2);
            for (char c : in) {
                if (std::strchr(".^$|()[]{}*+?\\/-", c)) out.push_back('\\');
                out.push_back(c);
            }
            return Value(out);
        }));

    interp.defineGlobal("re_find", Value::makeNativeFunction("re_find", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string text, pattern, flags;
            long long start = 0;
            if (!regexArgs(interp, args, "re_find", text, pattern, flags, 2)) return Value();
            if (args.size() >= 4 && args[3].isNumber()) start = static_cast<long long>(args[3].asNumber());
            if (start < 0) start = 0;
            if (start > static_cast<long long>(text.size())) return Value();

            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();

            std::smatch m;
            std::string::const_iterator begin = text.cbegin() + static_cast<size_t>(start);
            // match_prev_avail lets ^ and \b see the character before `start`,
            // so resuming a scan mid-string behaves like scanning the whole one.
            auto mflags = std::regex_constants::match_default;
            if (start > 0) mflags |= std::regex_constants::match_prev_avail;
            if (!std::regex_search(begin, text.cend(), m, re, mflags)) return Value();

            return makeMatchValue(m, static_cast<size_t>(start));
        }));

    interp.defineGlobal("re_find_all", Value::makeNativeFunction("re_find_all", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string text, pattern, flags;
            long long limit = 0;
            if (!regexArgs(interp, args, "re_find_all", text, pattern, flags, 2)) return Value();
            if (args.size() >= 4 && args[3].isNumber()) limit = static_cast<long long>(args[3].asNumber());

            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();

            std::vector<Value> out;
            size_t offset = 0;
            while (offset <= text.size()) {
                std::smatch m;
                auto mflags = std::regex_constants::match_default;
                if (offset > 0) mflags |= std::regex_constants::match_prev_avail;
                if (!std::regex_search(text.cbegin() + offset, text.cend(), m, re, mflags)) break;

                out.push_back(makeMatchValue(m, offset));
                if (limit > 0 && static_cast<long long>(out.size()) >= limit) break;

                size_t matchStart = offset + static_cast<size_t>(m.position(0));
                size_t matchEnd = matchStart + static_cast<size_t>(m.length(0));
                // A zero-width match (e.g. `a*` against "bbb") would otherwise
                // match forever at the same spot.
                offset = (matchEnd == matchStart) ? matchEnd + 1 : matchEnd;
            }
            return Value::makeArray(out);
        }));

    // re_replace(text, pattern, replacement, flags = "", limit = 0)
    // limit 0 replaces every match; 1 replaces the first only. $1..$9 and $& in
    // the replacement work as in ECMAScript.
    interp.defineGlobal("re_replace", Value::makeNativeFunction("re_replace", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 3 || !args[0].isString() || !args[1].isString() || !args[2].isString()) {
                interp.throwException("TypeError",
                    "re_replace() expects (text, pattern, replacement[, flags, limit])", 0, "");
                return Value();
            }
            std::string text = args[0].asString();
            std::string pattern = args[1].asString();
            std::string repl = args[2].asString();
            std::string flags = (args.size() >= 4 && args[3].isString()) ? args[3].asString() : "";
            long long limit = (args.size() >= 5 && args[4].isNumber())
                              ? static_cast<long long>(args[4].asNumber()) : 0;

            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();

            if (limit <= 0) {
                return Value(std::regex_replace(text, re, repl));
            }
            // Bounded replacement: std::regex_replace has no count, so walk the
            // matches and stop after `limit`.
            std::string out;
            size_t offset = 0;
            long long done = 0;
            while (offset <= text.size() && done < limit) {
                std::smatch m;
                auto mflags = std::regex_constants::match_default;
                if (offset > 0) mflags |= std::regex_constants::match_prev_avail;
                if (!std::regex_search(text.cbegin() + offset, text.cend(), m, re, mflags)) break;

                size_t matchStart = offset + static_cast<size_t>(m.position(0));
                size_t matchLen = static_cast<size_t>(m.length(0));
                out.append(text, offset, matchStart - offset);
                out.append(m.format(repl));
                done++;

                size_t next = matchStart + matchLen;
                if (matchLen == 0) {
                    if (next < text.size()) out.push_back(text[next]);
                    next += 1;
                }
                offset = next;
            }
            if (offset < text.size()) out.append(text, offset, std::string::npos);
            return Value(out);
        }));

    // re_split(text, pattern, flags = "", limit = 0)
    // limit > 0 caps the number of pieces; the remainder is left in the last one.
    interp.defineGlobal("re_split", Value::makeNativeFunction("re_split", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string text, pattern, flags;
            long long limit = 0;
            if (!regexArgs(interp, args, "re_split", text, pattern, flags, 2)) return Value();
            if (args.size() >= 4 && args[3].isNumber()) limit = static_cast<long long>(args[3].asNumber());

            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();

            std::vector<Value> parts;
            size_t offset = 0;
            size_t pieceStart = 0;
            while (offset <= text.size()) {
                if (limit > 0 && static_cast<long long>(parts.size()) + 1 >= limit) break;
                std::smatch m;
                auto mflags = std::regex_constants::match_default;
                if (offset > 0) mflags |= std::regex_constants::match_prev_avail;
                if (!std::regex_search(text.cbegin() + offset, text.cend(), m, re, mflags)) break;

                size_t matchStart = offset + static_cast<size_t>(m.position(0));
                size_t matchLen = static_cast<size_t>(m.length(0));
                if (matchLen == 0) {
                    // A zero-width separator would split between every character
                    // forever; step over one character instead.
                    offset = matchStart + 1;
                    if (offset > text.size()) break;
                    continue;
                }
                parts.push_back(Value(text.substr(pieceStart, matchStart - pieceStart)));
                pieceStart = matchStart + matchLen;
                offset = pieceStart;
            }
            parts.push_back(Value(text.substr(pieceStart)));
            return Value::makeArray(parts);
        }));

    // re_test(text, pattern, flags = "") -> bool   (searches anywhere)
    interp.defineGlobal("re_test", Value::makeNativeFunction("re_test", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string text, pattern, flags;
            if (!regexArgs(interp, args, "re_test", text, pattern, flags, 2)) return Value();
            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();
            return Value(std::regex_search(text, re));
        }));

    // re_full_match(text, pattern, flags = "") -> bool   (whole string)
    interp.defineGlobal("re_full_match", Value::makeNativeFunction("re_full_match", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            std::string text, pattern, flags;
            if (!regexArgs(interp, args, "re_full_match", text, pattern, flags, 2)) return Value();
            std::regex re;
            if (!buildRegex(interp, pattern, flags, re)) return Value();
            return Value(std::regex_match(text, re));
        }));

    // re_valid(pattern) -> bool   (does it compile?)
    interp.defineGlobal("re_valid", Value::makeNativeFunction("re_valid", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.throwException("TypeError", "re_valid() expects a string pattern", 0, "");
                return Value();
            }
            try {
                std::regex re(args[0].asString());
                (void)re;
                return Value(true);
            } catch (const std::regex_error&) {
                return Value(false);
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
                for (auto& v : args[0].asArray().getElementsCopy()) in += (char)v.asNumber();
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
