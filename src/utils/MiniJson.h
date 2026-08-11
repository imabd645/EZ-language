#ifndef MINIJSON_H
#define MINIJSON_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace MiniJson {
    enum Type { ALL_NULL, OBJECT, ARRAY, STRING, NUMBER, BOOLEAN };
    
    struct Value {
        Type type = ALL_NULL;
        std::map<std::string, Value> properties;
        std::vector<Value> items;
        std::string stringVal;
        
        Value() {}
        Value(Type t) : type(t) {}
        Value(const std::string& s) : type(STRING), stringVal(s) {}
        Value(const char* s) : type(STRING), stringVal(s) {}
        
        std::string asString() const { return stringVal; }
        
        bool isNull() const { return type == ALL_NULL; }
        
        Value get(const std::string& key, const Value& defaultValue) const {
            auto it = properties.find(key);
            if (it != properties.end()) return it->second;
            return defaultValue;
        }
        
        Value& operator[](const std::string& key) {
            if (type == ALL_NULL) type = OBJECT;
            return properties[key];
        }
        
        const Value& operator[](const std::string& key) const {
            auto it = properties.find(key);
            if (it != properties.end()) return it->second;
            static Value nullVal;
            return nullVal;
        }
        
        void append(const Value& val) {
            if (type == ALL_NULL) type = ARRAY;
            items.push_back(val);
        }
        
        std::vector<std::string> getMemberNames() const {
            std::vector<std::string> names;
            for (const auto& pair : properties) names.push_back(pair.first);
            return names;
        }
        
        std::vector<Value>::const_iterator begin() const { return items.begin(); }
        std::vector<Value>::const_iterator end() const { return items.end(); }
    };
    
    class Reader {
    public:
        bool parse(std::istream& is, Value& root) {
            std::string content((std::istreambuf_iterator<char>(is)), std::istreambuf_iterator<char>());
            return parse(content, root);
        }
        
        bool parse(const std::string& str, Value& root) {
            size_t pos = 0;
            skipWhitespace(str, pos);
            if (pos >= str.length()) return false;
            
            if (str[pos] == '{') root = parseObject(str, pos);
            else if (str[pos] == '[') root = parseArray(str, pos);
            else return false;
            return true;
        }
        
    private:
        void skipWhitespace(const std::string& str, size_t& pos) {
            while (pos < str.length() && isspace(str[pos])) pos++;
        }
        
        Value parseObject(const std::string& str, size_t& pos) {
            Value obj(OBJECT);
            pos++;

            while (pos < str.length()) {
                // Belt and braces: if an iteration ever consumes nothing, stop
                // rather than spin. parseValue() guarantees progress on its own,
                // but this loop is what actually hung the process and a parser
                // fed untrusted input should not be one edit away from doing it
                // again.
                size_t iterationStart = pos;

                skipWhitespace(str, pos);
                if (pos >= str.length()) break;
                if (str[pos] == '}') { pos++; break; }

                std::string key = parseString(str, pos);
                skipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == ':') pos++;
                skipWhitespace(str, pos);

                Value val = parseValue(str, pos);
                obj.properties[key] = val;

                skipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == ',') pos++;

                if (pos == iterationStart) break;
            }
            return obj;
        }
        
        Value parseArray(const std::string& str, size_t& pos) {
            Value arr(ARRAY);
            pos++;
            while (pos < str.length()) {
                size_t iterationStart = pos;   // see parseObject
                skipWhitespace(str, pos);
                if (pos >= str.length()) break;
                if (str[pos] == ']') { pos++; break; }
                arr.items.push_back(parseValue(str, pos));
                skipWhitespace(str, pos);
                if (pos < str.length() && str[pos] == ',') pos++;
                if (pos == iterationStart) break;
            }
            return arr;
        }
        
        Value parseValue(const std::string& str, size_t& pos) {
            skipWhitespace(str, pos);
            if (pos >= str.length()) return Value();
            if (str[pos] == '"') return Value(parseString(str, pos));
            if (str[pos] == '{') return parseObject(str, pos);
            if (str[pos] == '[') return parseArray(str, pos);

            // Numbers, true, false and null.
            //
            // isalnum() alone does not match a leading '-', so a negative value
            // consumed NOTHING: pos never moved, an empty token came back, and
            // the caller looped on the same character forever. `{"a": -5}` hung
            // the process outright -- and in a server that is the whole event
            // loop, so every later request hung with it.
            //
            // JSON numbers are  -? int frac? exp?  , so a sign is legal at the
            // front and directly after e/E.
            size_t start = pos;
            if (str[pos] == '-' || str[pos] == '+') pos++;
            while (pos < str.length()) {
                char c = str[pos];
                if (isalnum(static_cast<unsigned char>(c)) || c == '.') { pos++; continue; }
                if ((c == '-' || c == '+') && pos > start &&
                    (str[pos - 1] == 'e' || str[pos - 1] == 'E')) { pos++; continue; }
                break;
            }

            // Guarantee forward progress. Every loop that calls parseValue
            // assumes the position moves; if some byte we do not recognise ever
            // gets here, consuming it turns a hang into a parse error, which the
            // caller can at least report.
            if (pos == start) pos++;
            return Value(str.substr(start, pos - start));
        }
        
        // Decode one \uXXXX escape (already past the 'u') into `out`.
        // Returns false if the four hex digits are not there.
        static bool parseHex4(const std::string& str, size_t& pos, uint32_t& cp) {
            if (pos + 4 > str.length()) return false;
            cp = 0;
            for (int i = 0; i < 4; ++i) {
                char c = str[pos + i];
                cp <<= 4;
                if      (c >= '0' && c <= '9') cp |= (uint32_t)(c - '0');
                else if (c >= 'a' && c <= 'f') cp |= (uint32_t)(c - 'a' + 10);
                else if (c >= 'A' && c <= 'F') cp |= (uint32_t)(c - 'A' + 10);
                else return false;
            }
            pos += 4;
            return true;
        }

        static void appendUtf8(std::string& res, uint32_t cp) {
            if (cp < 0x80) {
                res += (char)cp;
            } else if (cp < 0x800) {
                res += (char)(0xC0 | (cp >> 6));
                res += (char)(0x80 | (cp & 0x3F));
            } else if (cp < 0x10000) {
                res += (char)(0xE0 | (cp >> 12));
                res += (char)(0x80 | ((cp >> 6) & 0x3F));
                res += (char)(0x80 | (cp & 0x3F));
            } else {
                res += (char)(0xF0 | (cp >> 18));
                res += (char)(0x80 | ((cp >> 12) & 0x3F));
                res += (char)(0x80 | ((cp >> 6) & 0x3F));
                res += (char)(0x80 | (cp & 0x3F));
            }
        }

        std::string parseString(const std::string& str, size_t& pos) {
            std::string res;
            if (str[pos] != '"') return "";
            pos++;
            // Escapes must be TRANSLATED, not just un-backslashed. This used to
            // skip the backslash and copy the next character verbatim, so `\n`
            // decoded to the letter 'n' and `\t` to 't' -- stringify() emits those
            // escapes for real control characters, so to_json -> parse_json did
            // not round-trip any string containing a newline or a tab, and every
            // \uXXXX arrived as the literal text "uXXXX".
            while (pos < str.length() && str[pos] != '"') {
                if (str[pos] != '\\') { res += str[pos++]; continue; }
                if (pos + 1 >= str.length()) { pos++; break; }
                char esc = str[pos + 1];
                pos += 2;
                switch (esc) {
                    case '"':  res += '"';  break;
                    case '\\': res += '\\'; break;
                    case '/':  res += '/';  break;
                    case 'b':  res += '\b'; break;
                    case 'f':  res += '\f'; break;
                    case 'n':  res += '\n'; break;
                    case 'r':  res += '\r'; break;
                    case 't':  res += '\t'; break;
                    case 'u': {
                        uint32_t cp = 0;
                        if (!parseHex4(str, pos, cp)) { res += 'u'; break; }
                        // A code point above the BMP arrives as a surrogate pair.
                        if (cp >= 0xD800 && cp <= 0xDBFF &&
                            pos + 1 < str.length() && str[pos] == '\\' && str[pos + 1] == 'u') {
                            size_t save = pos;
                            pos += 2;
                            uint32_t lo = 0;
                            if (parseHex4(str, pos, lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            } else {
                                pos = save;   // not a valid pair; emit the high half as-is
                            }
                        }
                        appendUtf8(res, cp);
                        break;
                    }
                    // Not a JSON escape: keep it literally rather than losing it.
                    default: res += '\\'; res += esc; break;
                }
            }
            if (pos < str.length()) pos++;
            return res;
        }
    };
    
    class StreamWriter {
    public:
        void write(const Value& root, std::ostream* os) {
            *os << stringify(root, 0);
        }
        
    private:
        std::string stringify(const Value& v, int indent) {
            std::string pad(indent, ' ');
            if (v.type == STRING) {
                std::string escaped = "\"";
                for (char c : v.stringVal) {
                    if (c == '"') escaped += "\\\"";
                    else if (c == '\\') escaped += "\\\\";
                    else if (c == '\n') escaped += "\\n";
                    else if (c == '\r') escaped += "\\r";
                    else if (c == '\t') escaped += "\\t";
                    else if (c == '\b') escaped += "\\b";
                    else if (c == '\f') escaped += "\\f";
                    // RFC 8259 forbids a raw control character inside a string, so
                    // the remaining ones ( -) have to go out as \uXXXX
                    // or the document we emit is not valid JSON.
                    else if ((unsigned char)c < 0x20) {
                        static const char* HEX = "0123456789abcdef";
                        escaped += "\\u00";
                        escaped += HEX[((unsigned char)c >> 4) & 0xF];
                        escaped += HEX[(unsigned char)c & 0xF];
                    }
                    else escaped += c;
                }
                escaped += "\"";
                return escaped;
            }
            if (v.type == OBJECT) {
                std::string s = "{\n";
                size_t i = 0;
                for (const auto& p : v.properties) {
                    s += pad + "  \"" + p.first + "\": " + stringify(p.second, indent + 2);
                    if (i++ < v.properties.size() - 1) s += ",";
                    s += "\n";
                }
                s += pad + "}";
                return s;
            }
            if (v.type == ARRAY) {
                std::string s = "[\n";
                for (size_t i = 0; i < v.items.size(); ++i) {
                    s += pad + "  " + stringify(v.items[i], indent + 2);
                    if (i < v.items.size() - 1) s += ",";
                    s += "\n";
                }
                s += pad + "]";
                return s;
            }
            if (v.type == NUMBER || v.type == BOOLEAN) {
                return v.stringVal;
            }
            if (v.type == ALL_NULL) {
                return "null";
            }
            return "\"" + v.stringVal + "\""; // Fallback
        }
    };
}

#endif // MINIJSON_H
