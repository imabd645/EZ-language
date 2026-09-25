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


void registerRegexBuiltins(RuntimeContext& interp) {
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

    interp.defineGlobal("re_test", Value::makeNativeFunction("re_test", -1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                std::string text, pattern, flags;
                if (!regexArgs(interp, args, "re_test", text, pattern, flags, 2)) return Value();
                std::regex re;
                if (!buildRegex(interp, pattern, flags, re)) return Value();
                return Value(std::regex_search(text, re));
            }));

    interp.defineGlobal("re_full_match", Value::makeNativeFunction("re_full_match", -1,
            [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
                std::string text, pattern, flags;
                if (!regexArgs(interp, args, "re_full_match", text, pattern, flags, 2)) return Value();
                std::regex re;
                if (!buildRegex(interp, pattern, flags, re)) return Value();
                return Value(std::regex_match(text, re));
            }));

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

}
