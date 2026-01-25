#ifndef MINIJSON_H
#define MINIJSON_H

#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <algorithm>
#include <iostream>

namespace MiniJson {
    enum Type { ALL_NULL, OBJECT, ARRAY, STRING };
    
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
                skipWhitespace(str, pos);
                if (str[pos] == '}') { pos++; break; }
                
                std::string key = parseString(str, pos);
                skipWhitespace(str, pos);
                if (str[pos] == ':') pos++;
                skipWhitespace(str, pos);
                
                Value val = parseValue(str, pos);
                obj.properties[key] = val;
                
                skipWhitespace(str, pos);
                if (str[pos] == ',') pos++;
            }
            return obj;
        }
        
        Value parseArray(const std::string& str, size_t& pos) {
            Value arr(ARRAY);
            pos++;
            while (pos < str.length()) {
                skipWhitespace(str, pos);
                if (str[pos] == ']') { pos++; break; }
                arr.items.push_back(parseValue(str, pos));
                skipWhitespace(str, pos);
                if (str[pos] == ',') pos++;
            }
            return arr;
        }
        
        Value parseValue(const std::string& str, size_t& pos) {
            skipWhitespace(str, pos);
            if (str[pos] == '"') return Value(parseString(str, pos));
            if (str[pos] == '{') return parseObject(str, pos);
            if (str[pos] == '[') return parseArray(str, pos);
            size_t start = pos;
            while (pos < str.length() && (isalnum(str[pos]) || str[pos] == '.')) pos++;
            return Value(str.substr(start, pos - start));
        }
        
        std::string parseString(const std::string& str, size_t& pos) {
            std::string res;
            if (str[pos] != '"') return "";
            pos++;
            while (pos < str.length() && str[pos] != '"') {
                if (str[pos] == '\\' && pos + 1 < str.length()) pos++;
                res += str[pos++];
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
            if (v.type == STRING) return "\"" + v.stringVal + "\"";
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
            return "\"" + v.stringVal + "\""; // Fallback
        }
    };
}

#endif // MINIJSON_H
