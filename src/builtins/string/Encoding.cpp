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
#include <sstream>
#include <iomanip>

void registerEncodingBuiltins(RuntimeContext& interp) {
static const char b64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

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
