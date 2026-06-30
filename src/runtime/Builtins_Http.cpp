#include "../Builtins.h"
#include "../RuntimeContext.h"
#include <string>
#include <vector>
#include <cctype>

void registerHttpBuiltins(RuntimeContext& interp) {
    interp.defineGlobal("http_parse_request", Value::makeNativeFunction("http_parse_request", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                interp.runtimeError("http_parse_request() expects string", 0, "");
                return Value();
            }
            std::string req = args[0].asString();
            
            size_t headerEnd = req.find("\r\n\r\n");
            if (headerEnd == std::string::npos) return Value(); // Incomplete headers
            
            size_t firstLBreak = req.find("\r\n");
            if (firstLBreak == std::string::npos) return Value();
            
            std::string firstLine = req.substr(0, firstLBreak);
            size_t s1 = firstLine.find(' ');
            size_t s2 = firstLine.find(' ', s1 + 1);
            
            std::string method = "";
            std::string path = "";
            if (s1 != std::string::npos && s2 != std::string::npos) {
                method = firstLine.substr(0, s1);
                path = firstLine.substr(s1 + 1, s2 - s1 - 1);
            }
            
            Value headers = Value::makeDictionary();
            auto& hmap = headers.asDictionary().map;
            
            size_t pos = firstLBreak + 2;
            int contentLen = 0;
            
            while (pos < headerEnd) {
                size_t lineEnd = req.find("\r\n", pos);
                if (lineEnd == std::string::npos) break;
                
                size_t colon = req.find(":", pos);
                if (colon != std::string::npos && colon < lineEnd) {
                    std::string key = req.substr(pos, colon - pos);
                    std::string val = req.substr(colon + 1, lineEnd - colon - 1);
                    
                    for (char& c : key) c = std::tolower(c);
                    val.erase(0, val.find_first_not_of(" \t"));
                    val.erase(val.find_last_not_of(" \t") + 1);
                    
                    hmap[key] = Value(val);
                    if (key == "content-length") {
                        try {
                            contentLen = std::stoi(val);
                        } catch (...) {}
                    }
                }
                pos = lineEnd + 2;
            }
            
            Value result = Value::makeDictionary();
            auto& rmap = result.asDictionary().map;
            rmap["method"] = Value(method);
            rmap["fullPath"] = Value(path);
            rmap["headers"] = headers;
            rmap["headerEnd"] = Value(static_cast<double>(headerEnd));
            rmap["contentLength"] = Value(static_cast<double>(contentLen));
            
            return result;
        }));
}
