#include "Builtins.h"
#include "Interpreter.h"
#include <iostream>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <map>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <conio.h>
#include "MiniJson.h"


#include <sqlite3.h>
#include <chrono>
#include <curl/curl.h>
#include <thread>
#include <future>

#include <future>
#include <iomanip>

struct SimplePDF {
    std::string filename;
    std::vector<long> offsets;
    std::stringstream body;
    std::stringstream currentStream;
    int pageCount = 0;
    std::vector<int> pageIds;
    std::vector<int> contentIds;
    int catalogId, pagesId, fontId;

    void begin(const std::string& fname) {
        filename = fname;
        body.str(""); body.clear();
        currentStream.str(""); currentStream.clear();
        offsets.clear();
        pageIds.clear();
        contentIds.clear();
        pageCount = 0;
        body << "%PDF-1.4\n";
    }

    int reserveId() {
        offsets.push_back(0);
        return (int)offsets.size();
    }

    void startObject(int id) {
        offsets[id - 1] = (long)body.tellp();
        body << id << " 0 obj\n";
    }

    void addPage() {
        if (pageCount > 0) finalizePage();
        pageCount++;
        pageIds.push_back(reserveId());
        contentIds.push_back(reserveId());
        currentStream.str(""); currentStream.clear();
    }

    void finalizePage() {
        int cId = contentIds.back();
        startObject(cId);
        std::string s = currentStream.str();
        body << "<< /Length " << s.length() << " >>\nstream\n" << s << "\nendstream\nendobj\n";
    }

    void save() {
        if (pageCount == 0) addPage();
        finalizePage();

        catalogId = reserveId();
        pagesId = reserveId();
        fontId = reserveId();

        // 1. Catalog
        startObject(catalogId);
        body << "<< /Type /Catalog /Pages " << pagesId << " 0 R >>\nendobj\n";

        // 2. Pages Root
        startObject(pagesId);
        body << "<< /Type /Pages /Kids [";
        for (int id : pageIds) body << id << " 0 R ";
        body << "] /Count " << pageCount << " >>\nendobj\n";

        // 3. Font
        startObject(fontId);
        body << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica /Name /F1 >>\nendobj\n";

        // 4. Page Objects
        for (size_t i = 0; i < pageIds.size(); ++i) {
            startObject(pageIds[i]);
            body << "<< /Type /Page /Parent " << pagesId << " 0 R /MediaBox [0 0 595 842] /Resources << /Font << /F1 " << fontId << " 0 R >> >> /Contents " << contentIds[i] << " 0 R >>\nendobj\n";
        }

        // 5. xref
        long startXref = (long)body.tellp();
        body << "xref\n0 " << (offsets.size() + 1) << "\n0000000000 65535 f \n";
        for (long off : offsets) {
            body << std::setw(10) << std::setfill('0') << off << " 00000 n \n";
        }

        // 6. trailer
        body << "trailer\n<< /Size " << (offsets.size() + 1) << " /Root " << catalogId << " 0 R >>\nstartxref\n" << startXref << "\n%%EOF";

        std::ofstream out(filename, std::ios::binary);
        out << body.str();
        out.close();
    }
};

static SimplePDF g_pdf;

void registerBuiltins(Interpreter& interp) {
    // clock() - returns milliseconds since epoch
    interp.defineGlobal("clock", Value::makeNativeFunction("clock", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            return Value((double)ms);
        }));

    // Input function
    interp.defineGlobal("__input__", Value::makeNativeFunction("input", 0, 
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    // len(x) - length of string or array
    interp.defineGlobal("len", Value::makeNativeFunction("len", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                return Value(static_cast<double>(args[0].asString().length()));
            }
            if (args[0].isArray()) {
                return Value(static_cast<double>(args[0].asArray().size()));
            }
            if (args[0].isDictionary()) {
                return Value(static_cast<double>(args[0].asDictionary().map.size()));
            }
            { interp.runtimeError("len() expects string or array", 0, ""); return Value();
         }}));
    
    // push(arr, val) - add element to array
    interp.defineGlobal("push", Value::makeNativeFunction("push", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("push() expects array as first argument", 0, ""); return Value();
             }}
            auto arr = args[0].asArrayPtr();
            arr->push_back(args[1]);
            return Value(arr);
        }));
    
    // pop(arr) - remove and return last element
    interp.defineGlobal("pop", Value::makeNativeFunction("pop", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("pop() expects array", 0, ""); return Value();
             }}
            auto& arr = *args[0].asArrayPtr();
            if (arr.empty()) {
                { interp.runtimeError("pop() on empty array", 0, ""); return Value();
             }}
            Value last = arr.back();
            arr.pop_back();
            return last;
        }));
    
    // str(x) - convert to string
    interp.defineGlobal("str", Value::makeNativeFunction("str", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return Value(interp.stringify(args[0]));
        }));
    
    // num(x) - convert to number
    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isNumber()) return args[0];
            if (args[0].isString()) {
                try {
                    return Value(std::stod(args[0].asString()));
                } catch (...) {
                    { interp.runtimeError("Cannot convert '" + args[0].asString() + "' to number", 0, ""); return Value();
                 }}
            }
            if (args[0].isBool()) {
                return Value(args[0].asBool() ? 1.0 : 0.0);
            }
            { interp.runtimeError("Cannot convert " + args[0].typeName() + " to number", 0, ""); return Value();
         }}));
    
    // type(x) - get type name
    interp.defineGlobal("type", Value::makeNativeFunction("type", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return Value(args[0].typeName());
        }));
    
    // substr(s, start, len) - get substring
    interp.defineGlobal("substr", Value::makeNativeFunction("substr", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("substr() expects string as first argument", 0, ""); return Value();
             }}
            if (!args[1].isNumber() || !args[2].isNumber()) {
                { interp.runtimeError("substr() expects numbers for start and length", 0, ""); return Value();
             }}
            const std::string& str = args[0].asString();
            int start = static_cast<int>(args[1].asNumber());
            int len = static_cast<int>(args[2].asNumber());
            
            if (start < 0) start = 0;
            if (start >= static_cast<int>(str.length())) return Value("");
            if (len < 0) len = 0;
            
            return Value(str.substr(start, len));
        }));
    
    // split(s, delim) - split string into array
    interp.defineGlobal("split", Value::makeNativeFunction("split", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) {
                { interp.runtimeError("split() expects two strings", 0, ""); return Value();
             }}
            
            const std::string& str = args[0].asString();
            const std::string& delim = args[1].asString();
            
            std::vector<Value> result;
            
            if (delim.empty()) {
                for (char c : str) {
                    result.push_back(Value(std::string(1, c)));
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
    
    // join(arr, delim) - join array into string
    interp.defineGlobal("join", Value::makeNativeFunction("join", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("join() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isString()) {
                { interp.runtimeError("join() expects string as delimiter", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            const std::string& delim = args[1].asString();
            
            std::string result;
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += delim;
                result += arr[i].toString();
            }
            
            return Value(result);
        }));
    
    // floor(x)
    interp.defineGlobal("floor", Value::makeNativeFunction("floor", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                { interp.runtimeError("floor() expects number", 0, ""); return Value();
             }}
            return Value(std::floor(args[0].asNumber()));
        }));
    
    // ceil(x)
    interp.defineGlobal("ceil", Value::makeNativeFunction("ceil", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                { interp.runtimeError("ceil() expects number", 0, ""); return Value();
             }}
            return Value(std::ceil(args[0].asNumber()));
        }));
    
    // abs(x)
    interp.defineGlobal("abs", Value::makeNativeFunction("abs", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                { interp.runtimeError("abs() expects number", 0, ""); return Value();
             }}
            return Value(std::abs(args[0].asNumber()));
        }));
    
    // sqrt(x)
    interp.defineGlobal("sqrt", Value::makeNativeFunction("sqrt", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                { interp.runtimeError("sqrt() expects number", 0, ""); return Value();
             }}
            double val = args[0].asNumber();
            if (val < 0) {
                { interp.runtimeError("sqrt() of negative number", 0, ""); return Value();
             }}
            return Value(std::sqrt(val));
        }));
    
    // pow(base, exp)
    interp.defineGlobal("pow", Value::makeNativeFunction("pow", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                { interp.runtimeError("pow() expects two numbers", 0, ""); return Value();
             }}
            return Value(std::pow(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // rand() - random number 0-1
    interp.defineGlobal("rand", Value::makeNativeFunction("rand", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            return Value(static_cast<double>(std::rand()) / RAND_MAX);
        }));
    
    // randint(min, max) - random integer in range
    interp.defineGlobal("randint", Value::makeNativeFunction("randint", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                { interp.runtimeError("randint() expects two numbers", 0, ""); return Value();
             }}
            int min = static_cast<int>(args[0].asNumber());
            int max = static_cast<int>(args[1].asNumber());
            return Value(static_cast<double>(min + std::rand() % (max - min + 1)));
        }));
    
    // round(x)
    interp.defineGlobal("round", Value::makeNativeFunction("round", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                { interp.runtimeError("round() expects number", 0, ""); return Value();
             }}
            return Value(std::round(args[0].asNumber()));
        }));
    
    // min(a, b)
    interp.defineGlobal("min", Value::makeNativeFunction("min", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                { interp.runtimeError("min() expects two numbers", 0, ""); return Value();
             }}
            return Value(std::min(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // max(a, b)
    interp.defineGlobal("max", Value::makeNativeFunction("max", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                { interp.runtimeError("max() expects two numbers", 0, ""); return Value();
             }}
            return Value(std::max(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // contains(str/arr, item) - check if string/array contains item
    interp.defineGlobal("contains", Value::makeNativeFunction("contains", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                if (!args[1].isString()) {
                    { interp.runtimeError("contains() with string expects string to search for", 0, ""); return Value();
                 }}
                return Value(args[0].asString().find(args[1].asString()) != std::string::npos);
            }
            if (args[0].isArray()) {
                const auto& arr = args[0].asArray();
                for (const auto& elem : arr) {
                    if (elem.equals(args[1])) {
                        return Value(true);
                    }
                }
                return Value(false);
            }
            if (args[0].isDictionary()) {
                std::string key = args[1].toString();
                const auto& dict = args[0].asDictionary();
                return Value(dict.map.find(key) != dict.map.end());
            }
            { interp.runtimeError("contains() expects string, array, or dictionary", 0, ""); return Value();
         }}));
    
    // indexOf(str/arr, item) - find index of item
    interp.defineGlobal("indexOf", Value::makeNativeFunction("indexOf", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                if (!args[1].isString()) {
                    { interp.runtimeError("indexOf() with string expects string to search for", 0, ""); return Value();
                 }}
                size_t pos = args[0].asString().find(args[1].asString());
                if (pos == std::string::npos) return Value(-1.0);
                return Value(static_cast<double>(pos));
            }
            if (args[0].isArray()) {
                const auto& arr = args[0].asArray();
                for (size_t i = 0; i < arr.size(); i++) {
                    if (arr[i].equals(args[1])) {
                        return Value(static_cast<double>(i));
                    }
                }
                return Value(-1.0);
            }
            { interp.runtimeError("indexOf() expects string or array", 0, ""); return Value();
         }}));
    
    // reverse(arr/str) - reverse array or string
    interp.defineGlobal("reverse", Value::makeNativeFunction("reverse", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                std::string s = args[0].asString();
                std::reverse(s.begin(), s.end());
                return Value(s);
            }
            if (args[0].isArray()) {
                auto arr = args[0].asArray();
                std::reverse(arr.begin(), arr.end());
                return Value::makeArray(arr);
            }
            { interp.runtimeError("reverse() expects string or array", 0, ""); return Value();
         }}));
    
    // sort(arr) - sort array
    interp.defineGlobal("sort", Value::makeNativeFunction("sort", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("sort() expects array", 0, ""); return Value();
             }}
            auto arr = args[0].asArray();
            std::sort(arr.begin(), arr.end(), [](const Value& a, const Value& b) {
                if (a.isNumber() && b.isNumber()) {
                    return a.asNumber() < b.asNumber();
                }
                return a.toString() < b.toString();
            });
            return Value::makeArray(arr);
        }));
    
    // upper(str) - uppercase
    interp.defineGlobal("upper", Value::makeNativeFunction("upper", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("upper() expects string", 0, ""); return Value();
             }}
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value(s);
        }));
    
    // lower(str) - lowercase
    interp.defineGlobal("lower", Value::makeNativeFunction("lower", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("lower() expects string", 0, ""); return Value();
             }}
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value(s);
        }));
    
    // trim(str) - trim whitespace
    interp.defineGlobal("trim", Value::makeNativeFunction("trim", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("trim() expects string", 0, ""); return Value();
             }}
            std::string s = args[0].asString();
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
            return Value(s);
        }));
    
    // replace(str, old, new) - replace substring
    interp.defineGlobal("replace", Value::makeNativeFunction("replace", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString() || !args[2].isString()) {
                { interp.runtimeError("replace() expects three strings", 0, ""); return Value();
             }}
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
    
    // startsWith(str, prefix) - check if string starts with prefix
    interp.defineGlobal("startsWith", Value::makeNativeFunction("startsWith", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) {
                { interp.runtimeError("startsWith() expects two strings", 0, ""); return Value();
             }}
            const std::string& str = args[0].asString();
            const std::string& prefix = args[1].asString();
            if (prefix.length() > str.length()) return Value(false);
            return Value(str.compare(0, prefix.length(), prefix) == 0);
        }));
    
    // endsWith(str, suffix) - check if string ends with suffix
    interp.defineGlobal("endsWith", Value::makeNativeFunction("endsWith", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) {
                { interp.runtimeError("endsWith() expects two strings", 0, ""); return Value();
             }}
            const std::string& str = args[0].asString();
            const std::string& suffix = args[1].asString();
            if (suffix.length() > str.length()) return Value(false);
            return Value(str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0);
        }));
    
    // has_key(dict, key) - check if dictionary contains a key
    interp.defineGlobal("has_key", Value::makeNativeFunction("has_key", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) {
                { interp.runtimeError("has_key() expects dictionary as first argument", 0, ""); return Value();
             }}
            std::string key = args[1].toString();
            const auto& map = args[0].asDictionary().map;
            return Value(map.find(key) != map.end());
        }));
    
    // remove(arr, index) - remove element at index, returns removed value
    interp.defineGlobal("remove", Value::makeNativeFunction("remove", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("remove() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isNumber()) {
                { interp.runtimeError("remove() expects number index as second argument", 0, ""); return Value();
             }}
            auto& arr = *args[0].asArrayPtr();
            int index = static_cast<int>(args[1].asNumber());
            if (index < 0 || index >= static_cast<int>(arr.size())) {
                { interp.runtimeError("remove() index out of bounds", 0, ""); return Value();
             }}
            Value removed = arr[index];
            arr.erase(arr.begin() + index);
            return removed;
        }));
    
    // insert(arr, index, value) - insert element at index
    interp.defineGlobal("insert", Value::makeNativeFunction("insert", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("insert() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isNumber()) {
                { interp.runtimeError("insert() expects number index as second argument", 0, ""); return Value();
             }}
            auto& arr = *args[0].asArrayPtr();
            int index = static_cast<int>(args[1].asNumber());
            if (index < 0 || index > static_cast<int>(arr.size())) {
                { interp.runtimeError("insert() index out of bounds", 0, ""); return Value();
             }}
            arr.insert(arr.begin() + index, args[2]);
            return args[0];
        }));
    
    // slice(arr/str, start, end) - get slice
    interp.defineGlobal("slice", Value::makeNativeFunction("slice", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[1].isNumber() || !args[2].isNumber()) {
                { interp.runtimeError("slice() expects numbers for start and end", 0, ""); return Value();
             }}
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
                return Value::makeArray(std::vector<Value>(arr.begin() + start, arr.begin() + end));
            }
            { interp.runtimeError("slice() expects string or array", 0, ""); return Value();
         }}));
    
    // print (alias for out but as function)
    interp.defineGlobal("print", Value::makeNativeFunction("print", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value();
        }));
    
    // input(prompt) - input with optional prompt
    interp.defineGlobal("input", Value::makeNativeFunction("input", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    // range(end) or range(start, end) - create array from range
    interp.defineGlobal("range", Value::makeNativeFunction("range", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || args.size() > 2) {
                { interp.runtimeError("range() expects 1 or 2 arguments", 0, ""); return Value();
             }}
            
            int start = 0, end = 0;
            if (args.size() == 1) {
                if (!args[0].isNumber()) { interp.runtimeError("range() expects number", 0, ""); return Value();
                 }end = static_cast<int>(args[0].asNumber());
            } else {
                if (!args[0].isNumber() || !args[1].isNumber()) {
                    { interp.runtimeError("range() expects numbers", 0, ""); return Value();
                 }}
                start = static_cast<int>(args[0].asNumber());
                end = static_cast<int>(args[1].asNumber());
            }
            
            std::vector<Value> result;
            for (int i = start; i < end; i++) {
                result.push_back(Value(static_cast<double>(i)));
            }
            return Value::makeArray(result);
        }));
    
    // map(arr, fn) - apply function to each element
    interp.defineGlobal("map", Value::makeNativeFunction("map", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("map() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("map() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            
            for (const auto& elem : arr) {
                result.push_back(interp.callFunction(args[1], {elem}, 0, "native"));
            }
            
            return Value::makeArray(result);
        }));
    
    // filter(arr, fn) - filter elements where fn returns truthy
    interp.defineGlobal("filter", Value::makeNativeFunction("filter", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("filter() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("filter() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0, "native");
                if (test.isTruthy()) {
                    result.push_back(elem);
                }
            }
            
            return Value::makeArray(result);
        }));
    
    // reduce(arr, fn, initial) - reduce array to single value
    interp.defineGlobal("reduce", Value::makeNativeFunction("reduce", 3,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("reduce() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("reduce() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            Value acc = args[2];
            
            for (const auto& elem : arr) {
                acc = interp.callFunction(args[1], {acc, elem}, 0, "native");
            }
            
            return acc;
        }));
    
    // forEach(arr, fn) - apply function to each element (no return)
    interp.defineGlobal("forEach", Value::makeNativeFunction("forEach", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("forEach() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("forEach() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                interp.callFunction(args[1], {elem}, 0, "native");
            }
            
            return Value();
        }));
    
    // find(arr, fn) - find first element where fn returns truthy
    interp.defineGlobal("find", Value::makeNativeFunction("find", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("find() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("find() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0, "native");
                if (test.isTruthy()) {
                    return elem;
                }
            }
            
            return Value();  // nil if not found
        }));
    
    // every(arr, fn) - true if fn returns truthy for all elements
    interp.defineGlobal("every", Value::makeNativeFunction("every", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("every() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("every() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0, "native");
                if (!test.isTruthy()) {
                    return Value(false);
                }
            }
            
            return Value(true);
        }));
    
    // some(arr, fn) - true if fn returns truthy for at least one element
    interp.defineGlobal("some", Value::makeNativeFunction("some", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                { interp.runtimeError("some() expects array as first argument", 0, ""); return Value();
             }}
            if (!args[1].isCallable()) {
                { interp.runtimeError("some() expects function as second argument", 0, ""); return Value();
             }}
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0, "native");
                if (test.isTruthy()) {
                    return Value(true);
                }
            }
            
            return Value(false);
        }));
    
    // readFile(path) - read file content as string
    interp.defineGlobal("readFile", Value::makeNativeFunction("readFile", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("readFile() expects string path", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "'", 0, ""); return Value();
             }}
            std::stringstream buffer;
            buffer << file.rdbuf();
            return Value(buffer.str());
        }));
    
    // writeFile(path, content) - write string to file
    interp.defineGlobal("writeFile", Value::makeNativeFunction("writeFile", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("writeFile() expects string path", 0, ""); return Value();
             }}
            if (!args[1].isString()) {
                { interp.runtimeError("writeFile() expects string content", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "' for writing", 0, ""); return Value();
             }}
            file << content;
            return Value(true);
        }));
    
    // appendFile(path, content) - append string to file
    interp.defineGlobal("appendFile", Value::makeNativeFunction("appendFile", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("appendFile() expects string path", 0, ""); return Value();
             }}
            if (!args[1].isString()) {
                { interp.runtimeError("appendFile() expects string content", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "' for appending", 0, ""); return Value();
             }}
            file << content;
            return Value(true);
        }));
    
    // readLines(path) - read file into array of lines
    interp.defineGlobal("readLines", Value::makeNativeFunction("readLines", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("readLines() expects string path", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "'", 0, ""); return Value();
             }}
            std::vector<Value> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(Value(line));
            }
            return Value::makeArray(lines);
        }));
    
    // writeLine(path, content) - write string with newline to file
    interp.defineGlobal("writeLine", Value::makeNativeFunction("writeLine", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("writeLine() expects string path", 0, ""); return Value();
             }}
            if (!args[1].isString()) {
                { interp.runtimeError("writeLine() expects string content", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "' for writing", 0, ""); return Value();
             }}
            file << content << std::endl;
            return Value(true);
        }));
    
    // appendLine(path, content) - append string with newline to file
    interp.defineGlobal("appendLine", Value::makeNativeFunction("appendLine", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                { interp.runtimeError("appendLine() expects string path", 0, ""); return Value();
             }}
            if (!args[1].isString()) {
                { interp.runtimeError("appendLine() expects string content", 0, ""); return Value();
             }}
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) {
                { interp.runtimeError("Could not open file '" + path + "' for appending", 0, ""); return Value();
             }}
            file << content << std::endl;
            return Value(true);
        }));
    
    // keys(dict)
    interp.defineGlobal("keys", Value::makeNativeFunction("keys", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("keys() expects dictionary", 0, ""); return Value();
             }const auto& map = args[0].asDictionary().map;
            std::vector<Value> keys;
            for (const auto& kv : map) {
                keys.push_back(Value(kv.first));
            }
            return Value::makeArray(keys);
        }));
    
    // values(dict)
    interp.defineGlobal("values", Value::makeNativeFunction("values", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("values() expects dictionary", 0, ""); return Value();
             }const auto& map = args[0].asDictionary().map;
            std::vector<Value> vals;
            for (const auto& kv : map) {
                vals.push_back(kv.second);
            }
            return Value::makeArray(vals);
        }));

#ifdef _WIN32
    // server(port, handler) - Windows-only web server
    interp.defineGlobal("server", Value::makeNativeFunction("server", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("server() port must be a number", 0, ""); return Value();
             }if (!args[1].isFunction()) { interp.runtimeError("server() handler must be a function", 0, ""); return Value();
            
             }int port = static_cast<int>(args[0].asNumber());
            Value handler = args[1];

            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                { interp.runtimeError("WSAStartup failed", 0, ""); return Value();
             }}

            SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket == INVALID_SOCKET) {
                WSACleanup();
                { interp.runtimeError("Socket creation failed", 0, ""); return Value();
             }}

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);

            if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                closesocket(listenSocket);
                WSACleanup();
                { interp.runtimeError("Bind failed", 0, ""); return Value();
             }}

            if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
                closesocket(listenSocket);
                WSACleanup();
                { interp.runtimeError("Listen failed", 0, ""); return Value();
             }}

            while (true) {
                SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket == INVALID_SOCKET) break;

                // Capture globalEnv to share with the new thread's interpreter
                auto globalEnv = interp.getGlobalEnv();
                
                // Spawn a detached thread for each client
                std::thread([clientSocket, handler, globalEnv]() {
                    // OPTIMIZATION: Create a child environment for this request
                    // and use the lightweight constructor to skip overhead
                    auto requestEnv = globalEnv->createChild();
                    Interpreter threadInterp(requestEnv);

                    // Read headers
                    std::string request;
                    char buffer[4096];
                    bool headersComplete = false;
                    size_t contentLen = 0;
                    
                    while (!headersComplete) {
                        int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                        if (bytesRead <= 0) break;
                        request.append(buffer, bytesRead);
                        
                        size_t headerEnd = request.find("\r\n\r\n");
                        if (headerEnd != std::string::npos) {
                            headersComplete = true;
                            
                            // Parse Content-Length
                            size_t clPos = request.find("Content-Length: ");
                            if (clPos != std::string::npos) {
                                size_t start = clPos + 16;
                                size_t end = request.find("\r\n", start);
                                if (end != std::string::npos) {
                                    contentLen = std::stoi(request.substr(start, end - start));
                                }
                            }
                        }
                    }

                    if (headersComplete) {
                        // Check if we have the full body
                        size_t headerEnd = request.find("\r\n\r\n");
                        size_t bodyStart = headerEnd + 4;
                        
                        while (request.length() - bodyStart < contentLen) {
                            int bytesRead = recv(clientSocket, buffer, sizeof(buffer), 0);
                            if (bytesRead <= 0) break;
                            request.append(buffer, bytesRead);
                        }
                        
                        std::string method, fullPath, version, body;
                        std::unordered_map<std::string, Value> headers;
                        std::unordered_map<std::string, Value> query;

                        // Parse Request Line
                        size_t firstSpace = request.find(' ');
                        if (firstSpace != std::string::npos) {
                            method = request.substr(0, firstSpace);
                            size_t secondSpace = request.find(' ', firstSpace + 1);
                            if (secondSpace != std::string::npos) {
                                fullPath = request.substr(firstSpace + 1, secondSpace - (firstSpace + 1));
                                version = request.substr(secondSpace + 1, headerEnd - (secondSpace + 1)); // Approximate
                            }
                        }

                        // Parse Path and Query Params
                        std::string path = fullPath;
                        size_t qPos = fullPath.find('?');
                        if (qPos != std::string::npos) {
                            path = fullPath.substr(0, qPos);
                            std::string qStr = fullPath.substr(qPos + 1);
                            size_t start = 0;
                            while (start < qStr.length()) {
                                size_t amPos = qStr.find('&', start);
                                std::string pair = qStr.substr(start, amPos == std::string::npos ? amPos : amPos - start);
                                size_t eqPos = pair.find('=');
                                if (eqPos != std::string::npos) {
                                    query[pair.substr(0, eqPos)] = Value(pair.substr(eqPos + 1));
                                } else if (!pair.empty()) {
                                    query[pair] = Value(true);
                                }
                                if (amPos == std::string::npos) break;
                                start = amPos + 1;
                            }
                        }
                        
                        // Body
                        if (request.length() > bodyStart) {
                            body = request.substr(bodyStart);
                        }
                        
                        // Parse Headers
                        size_t pos = request.find("\r\n") + 2;
                        while (pos < headerEnd) {
                            size_t nextLine = request.find("\r\n", pos);
                            if (nextLine == std::string::npos || nextLine > headerEnd) break;
                            
                            std::string line = request.substr(pos, nextLine - pos);
                            size_t colon = line.find(':');
                            if (colon != std::string::npos) {
                                std::string k = line.substr(0, colon);
                                std::string v = line.substr(colon + 1);
                                v.erase(0, v.find_first_not_of(" "));
                                headers[k] = Value(v);
                            }
                            pos = nextLine + 2;
                        }


                        // Parse Form Data if application/x-www-form-urlencoded
                        std::unordered_map<std::string, Value> formData;
                        if (headers.count("Content-Type") && headers["Content-Type"].asString().find("application/x-www-form-urlencoded") != std::string::npos) {
                            std::string qStr = body;
                            size_t start = 0;
                            while (start < qStr.length()) {
                                size_t amPos = qStr.find('&', start);
                                std::string pair = qStr.substr(start, amPos == std::string::npos ? amPos : amPos - start);
                                size_t eqPos = pair.find('=');
                                if (eqPos != std::string::npos) {
                                    std::string key = pair.substr(0, eqPos);
                                    std::string val = pair.substr(eqPos + 1);
                                    
                                    // Replace + with space
                                    std::replace(key.begin(), key.end(), '+', ' ');
                                    std::replace(val.begin(), val.end(), '+', ' ');
                                    
                                    // URL Decode using CURL
                                    CURL* curl = curl_easy_init();
                                    if (curl) {
                                        int outlen;
                                        char* uns_key = curl_easy_unescape(curl, key.c_str(), (int)key.length(), &outlen);
                                        std::string dec_key(uns_key, outlen);
                                        curl_free(uns_key);
                                        
                                        char* uns_val = curl_easy_unescape(curl, val.c_str(), (int)val.length(), &outlen);
                                        std::string dec_val(uns_val, outlen);
                                        curl_free(uns_val);
                                        
                                        curl_easy_cleanup(curl);
                                        formData[dec_key] = Value(dec_val);
                                    }
                                }
                                if (amPos == std::string::npos) break;
                                start = amPos + 1;
                            }
                        }

                        // Create request object
                        Value reqArg = Value::makeDictionary();
                        auto& reqMap = reqArg.asDictionary().map;
                        reqMap["method"] = Value(method);
                        reqMap["path"] = Value(path);
                        reqMap["fullPath"] = Value(fullPath);
                        reqMap["version"] = Value(version);
                        reqMap["body"] = Value(body);
                        
                        Value formDict = Value::makeDictionary();
                        formDict.asDictionary().map = std::move(formData);
                        reqMap["form"] = formDict;

                        Value queryDict = Value::makeDictionary();
                        queryDict.asDictionary().map = std::move(query);
                        reqMap["query"] = queryDict;

                        Value headerDict = Value::makeDictionary();
                        headerDict.asDictionary().map = std::move(headers);
                        reqMap["headers"] = headerDict;

                        std::vector<Value> callbackArgs = {reqArg};
                        try {
                            Value result = threadInterp.callFunction(handler, callbackArgs, 0, "native");
                            std::string respStr;
                            
                            if (result.isDictionary()) {
                                auto& d = result.asDictionary().map;
                                int status = d.count("status") ? (int)d.at("status").asNumber() : 200;
                                std::string b = d.count("body") ? d.at("body").toString() : "";
                                
                                respStr = "HTTP/1.1 " + std::to_string(status) + " OK\r\n";
                                if (d.count("headers") && d.at("headers").isDictionary()) {
                                    for (auto& kv : d.at("headers").asDictionary().map) {
                                        respStr += kv.first + ": " + kv.second.toString() + "\r\n";
                                    }
                                } else {
                                    respStr += "Content-Type: text/html\r\n";
                                }
                                respStr += "Content-Length: " + std::to_string(b.length()) + "\r\n";
                                respStr += "\r\n";
                                respStr += b;
                            } else {
                                respStr = result.toString();
                                if (respStr.find("HTTP/") != 0) {
                                    std::string b = respStr;
                                    respStr = "HTTP/1.1 200 OK\r\n";
                                    respStr += "Content-Type: text/html\r\n";
                                    respStr += "Content-Length: " + std::to_string(b.length()) + "\r\n";
                                    respStr += "\r\n";
                                    respStr += b;
                                }
                            }
                            
                            send(clientSocket, respStr.c_str(), (int)respStr.length(), 0);
                        } catch (const std::exception& e) {
                            std::string errResp = "HTTP/1.1 500 Internal Server Error\r\n\r\nServer Error: " + std::string(e.what());
                            send(clientSocket, errResp.c_str(), (int)errResp.length(), 0);
                        }
                    }
                    closesocket(clientSocket);
                }).detach();
            }

            closesocket(listenSocket);
            WSACleanup();
            return Value();
        }));
#else
    interp.defineGlobal("server", Value::makeNativeFunction("server", 2,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            { interp.runtimeError("server() is only supported on Windows", 0, ""); return Value();
         }}));
#endif

    // serveFile(path) - helper to serve a file with correct headers
    interp.defineGlobal("serveFile", Value::makeNativeFunction("serveFile", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("serveFile() expects string path", 0, ""); return Value();
             }std::string path = args[0].asString();
            
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) {
                Value resp = Value::makeDictionary();
                resp.asDictionary().map["status"] = Value(404.0);
                resp.asDictionary().map["body"] = Value("File not found: " + path);
                return resp;
            }
            
            std::stringstream buffer;
            buffer << file.rdbuf();
            std::string body = buffer.str();
            
            std::string ext = "";
            size_t dot = path.find_last_of('.');
            if (dot != std::string::npos) ext = path.substr(dot + 1);
            
            std::string mime = "text/plain";
            if (ext == "html" || ext == "htm") mime = "text/html";
            else if (ext == "css") mime = "text/css";
            else if (ext == "js") mime = "text/javascript";
            else if (ext == "png") mime = "image/png";
            else if (ext == "jpg" || ext == "jpeg") mime = "image/jpeg";
            else if (ext == "json") mime = "application/json";
            
            Value resp = Value::makeDictionary();
            auto& d = resp.asDictionary().map;
            d["status"] = Value(200.0);
            
            Value headers = Value::makeDictionary();
            headers.asDictionary().map["Content-Type"] = Value(mime);
            d["headers"] = headers;
            
            d["body"] = Value(body);
            return resp;
        }));

    // Database functions
    static std::unordered_map<int, sqlite3*> dbConnections;
    static int nextDbHandle = 1;

    // dbOpen(path)
    interp.defineGlobal("dbOpen", Value::makeNativeFunction("dbOpen", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("dbOpen() expects string path", 0, ""); return Value();
             }std::string path = args[0].asString();
            
            sqlite3* db;
            int rc = sqlite3_open(path.c_str(), &db);
            if (rc != SQLITE_OK) {
                std::string err = sqlite3_errmsg(db);
                sqlite3_close(db);
                { interp.runtimeError("sqlite3_open failed: " + err, 0, ""); return Value();
             }}
            
            int handle = nextDbHandle++;
            dbConnections[handle] = db;
            return Value((double)handle);
        }));

    // dbExec(handle, sql, [params])
    interp.defineGlobal("dbExec", Value::makeNativeFunction("dbExec", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("dbExec() expects at least 2 arguments", 0, ""); return Value();
             }if (!args[0].isNumber()) { interp.runtimeError("dbExec() expects number handle", 0, ""); return Value();
             }if (!args[1].isString()) { interp.runtimeError("dbExec() expects string SQL", 0, ""); return Value();
            
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }}
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                { interp.runtimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
             }}
            
            // Bind parameters
            if (args.size() > 2 && args[2].isArray()) {
                const auto& params = args[2].asArray();
                for (int i = 0; i < (int)params.size(); i++) {
                    const auto& p = params[i];
                    int idx = i + 1;
                    if (p.isNil()) sqlite3_bind_null(stmt, idx);
                    else if (p.isBool()) sqlite3_bind_int(stmt, idx, p.asBool() ? 1 : 0);
                    else if (p.isNumber()) sqlite3_bind_double(stmt, idx, p.asNumber());
                    else if (p.isString()) sqlite3_bind_text(stmt, idx, p.asString().c_str(), -1, SQLITE_TRANSIENT);
                    else sqlite3_bind_text(stmt, idx, p.toString().c_str(), -1, SQLITE_TRANSIENT);
                }
            }
            
            int rc = sqlite3_step(stmt);
            sqlite3_finalize(stmt);
            
            if (rc != SQLITE_DONE && rc != SQLITE_ROW) {
                { interp.runtimeError("sqlite3_step failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
             }}
            
            return Value(true);
        }));

    // dbQuery(handle, sql, [params])
    interp.defineGlobal("dbQuery", Value::makeNativeFunction("dbQuery", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("dbQuery() expects at least 2 arguments", 0, ""); return Value();
             }if (!args[0].isNumber()) { interp.runtimeError("dbQuery() expects number handle", 0, ""); return Value();
             }if (!args[1].isString()) { interp.runtimeError("dbQuery() expects string SQL", 0, ""); return Value();
            
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }}
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                { interp.runtimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)), 0, ""); return Value();
             }}
            
            // Bind parameters
            if (args.size() > 2 && args[2].isArray()) {
                const auto& params = args[2].asArray();
                for (int i = 0; i < (int)params.size(); i++) {
                    const auto& p = params[i];
                    int idx = i + 1;
                    if (p.isNil()) sqlite3_bind_null(stmt, idx);
                    else if (p.isBool()) sqlite3_bind_int(stmt, idx, p.asBool() ? 1 : 0);
                    else if (p.isNumber()) sqlite3_bind_double(stmt, idx, p.asNumber());
                    else if (p.isString()) sqlite3_bind_text(stmt, idx, p.asString().c_str(), -1, SQLITE_TRANSIENT);
                    else sqlite3_bind_text(stmt, idx, p.toString().c_str(), -1, SQLITE_TRANSIENT);
                }
            }
            
            std::vector<Value> results;
            int colCount = sqlite3_column_count(stmt);
            
            while (sqlite3_step(stmt) == SQLITE_ROW) {
                Value row = Value::makeDictionary();
                auto& rowMap = row.asDictionary().map;
                
                for (int i = 0; i < colCount; i++) {
                    const char* name = sqlite3_column_name(stmt, i);
                    std::string colName = name ? name : "col_" + std::to_string(i);
                    int type = sqlite3_column_type(stmt, i);
                    
                    if (type == SQLITE_INTEGER) {
                        rowMap[colName] = Value((double)sqlite3_column_int64(stmt, i));
                    } else if (type == SQLITE_FLOAT) {
                        rowMap[colName] = Value(sqlite3_column_double(stmt, i));
                    } else if (type == SQLITE_TEXT) {
                        const char* text = (const char*)sqlite3_column_text(stmt, i);
                        rowMap[colName] = Value(text ? text : "");
                    } else if (type == SQLITE_NULL) {
                        rowMap[colName] = Value();
                    } else {
                        const char* text = (const char*)sqlite3_column_text(stmt, i);
                        rowMap[colName] = Value(text ? text : "");
                    }
                }
                results.push_back(row);
            }
            
            sqlite3_finalize(stmt);
            return Value::makeArray(results);
        }));

    // dbClose(handle)
    interp.defineGlobal("dbClose", Value::makeNativeFunction("dbClose", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbClose() expects number handle", 0, ""); return Value();
            
             }int handle = (int)args[0].asNumber();
            auto it = dbConnections.find(handle);
            if (it != dbConnections.end()) {
                sqlite3_close(it->second);
                dbConnections.erase(it);
            }
            return Value(true);
        }));

    // dbLastInsertId(handle)
    interp.defineGlobal("dbLastInsertId", Value::makeNativeFunction("dbLastInsertId", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbLastInsertId() expects number handle", 0, ""); return Value();
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }return Value((double)sqlite3_last_insert_rowid(dbConnections[handle]));
        }));

    // dbBegin(handle)
    interp.defineGlobal("dbBegin", Value::makeNativeFunction("dbBegin", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbBegin() expects number handle", 0, ""); return Value();
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }sqlite3_exec(dbConnections[handle], "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // dbCommit(handle)
    interp.defineGlobal("dbCommit", Value::makeNativeFunction("dbCommit", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbCommit() expects number handle", 0, ""); return Value();
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }sqlite3_exec(dbConnections[handle], "COMMIT", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // dbRollback(handle)
    interp.defineGlobal("dbRollback", Value::makeNativeFunction("dbRollback", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("dbRollback() expects number handle", 0, ""); return Value();
             }int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) { interp.runtimeError("Invalid database handle", 0, ""); return Value();
             }sqlite3_exec(dbConnections[handle], "ROLLBACK", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // ord(str) - returns ASCII value of first char
    interp.defineGlobal("ord", Value::makeNativeFunction("ord", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("ord() expects string", 0, ""); return Value();
             }std::string s = args[0].asString();
            if (s.empty()) return Value(0.0);
            return Value((double)(unsigned char)s[0]);
        }));

    // chr(num) - returns char from ASCII value
    interp.defineGlobal("chr", Value::makeNativeFunction("chr", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("chr() expects number", 0, ""); return Value();
             }char c = (char)(int)args[0].asNumber();
            return Value(std::string(1, c));
        }));

    // pdf_text(size, x, y, text)
    interp.defineGlobal("pdf_text", Value::makeNativeFunction("pdf_text", 4,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("pdf_text() expects number size", 0, ""); return Value();
             }if (!args[1].isNumber()) { interp.runtimeError("pdf_text() expects number x", 0, ""); return Value();
             }if (!args[2].isNumber()) { interp.runtimeError("pdf_text() expects number y", 0, ""); return Value();
             }if (!args[3].isString()) { interp.runtimeError("pdf_text() expects string text", 0, ""); return Value();

             }int size = (int)args[0].asNumber();
            int x = (int)args[1].asNumber();
            int y = (int)args[2].asNumber();
            std::string text = args[3].asString();

            // For now, we append to a global stream that the last page uses
            // In a better impl, we'd have a currentStream
            g_pdf.currentStream << "BT /F1 " << size << " Tf " << x << " " << y << " Td (" << text << ") Tj ET\n";
            return Value();
        }));

    interp.defineGlobal("pdf_line", Value::makeNativeFunction("pdf_line", 4,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("pdf_line() expects number x1", 0, ""); return Value();
             }if (!args[1].isNumber()) { interp.runtimeError("pdf_line() expects number y1", 0, ""); return Value();
             }if (!args[2].isNumber()) { interp.runtimeError("pdf_line() expects number x2", 0, ""); return Value();
             }if (!args[3].isNumber()) { interp.runtimeError("pdf_line() expects number y2", 0, ""); return Value();

             }int x1 = (int)args[0].asNumber();
            int y1 = (int)args[1].asNumber();
            int x2 = (int)args[2].asNumber();
            int y2 = (int)args[3].asNumber();
            g_pdf.currentStream << x1 << " " << y1 << " m " << x2 << " " << y2 << " l s\n";
            return Value();
        }));

    // xor(a, b) - bitwise XOR
    interp.defineGlobal("xor", Value::makeNativeFunction("xor", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) { interp.runtimeError("xor() expects numbers", 0, ""); return Value();
             }int a = (int)args[0].asNumber();
            int b = (int)args[1].asNumber();
            return Value((double)(a ^ b));
        }));

    // substring(str, start, [len])
    interp.defineGlobal("substring", Value::makeNativeFunction("substring", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) { interp.runtimeError("substring() expects 2 or 3 arguments", 0, ""); return Value();
             }if (!args[0].isString()) { interp.runtimeError("substring() first arg must be string", 0, ""); return Value();
             }if (!args[1].isNumber()) { interp.runtimeError("substring() start must be number", 0, ""); return Value();
            
             }std::string s = args[0].asString();
            int start = (int)args[1].asNumber();
            int len = (args.size() == 3 && args[2].isNumber()) ? (int)args[2].asNumber() : (int)s.length() - start;
            
            if (start < 0) start = 0;
            if (start > (int)s.length()) return Value("");
            if (len < 0) len = 0;
            
            return Value(s.substr(start, len));
        }));

    // NOTE: split, join, push, pop are already registered above (lines 60-185).
    // Duplicate registrations were removed to prevent silent overwrites.

    // toLower(str)
    interp.defineGlobal("toLower", Value::makeNativeFunction("toLower", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toLower() expects string", 0, ""); return Value();
             }std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    // toUpper(str)
    interp.defineGlobal("toUpper", Value::makeNativeFunction("toUpper", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("toUpper() expects string", 0, ""); return Value();
             }std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    // typeOf(val)
    interp.defineGlobal("typeOf", Value::makeNativeFunction("typeOf", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            return Value(args[0].typeName());
        }));

    // dictRemove(dict, key)
    interp.defineGlobal("dictRemove", Value::makeNativeFunction("dictRemove", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) { interp.runtimeError("dictRemove() expects dictionary", 0, ""); return Value();
             }std::string key = args[1].toString();
            args[0].asDictionaryPtr()->map.erase(key);
            return args[0];
        }));

    // --- Added Missing Functions ---

    // stop(ms) - Sleep for specified milliseconds
    interp.defineGlobal("stop", Value::makeNativeFunction("stop", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("stop() expects number", 0, ""); return Value();
             }std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asNumber()));
            return Value();
        }));

    // parse_json(str) - Convert JSON string to EZ value
    interp.defineGlobal("parse_json", Value::makeNativeFunction("parse_json", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("parse_json() expects string", 0, ""); return Value();
             }MiniJson::Value root;
            MiniJson::Reader reader;
            if (!reader.parse(args[0].asString(), root)) {
                { interp.runtimeError("Failed to parse JSON", 0, ""); return Value();
             }}
            
            std::function<Value(const MiniJson::Value&)> convert;
            convert = [&](const MiniJson::Value& mv) -> Value {
                if (mv.type == MiniJson::OBJECT) {
                    Value dv = Value::makeDictionary();
                    auto& map = dv.asDictionary().map;
                    for (const auto& name : mv.getMemberNames()) {
                        map[name] = convert(mv[name]);
                    }
                    return dv;
                } else if (mv.type == MiniJson::ARRAY) {
                    std::vector<Value> av;
                    for (const auto& item : mv.items) av.push_back(convert(item));
                    return Value::makeArray(av);
                } else {
                    std::string s = mv.asString();
                    if (s == "true") return Value(true);
                    if (s == "false") return Value(false);
                    if (s == "null") return Value();
                    // Try number
                    if (!s.empty() && (isdigit(s[0]) || s[0] == '-' || s[0] == '.')) {
                        try {
                            size_t pos;
                            double d = std::stod(s, &pos);
                            if (pos == s.length()) return Value(d);
                        } catch (...) {}
                    }
                    return Value(s);
                }
            };
            return convert(root);
        }));

    // to_json(val) - Convert EZ value to JSON string
    interp.defineGlobal("to_json", Value::makeNativeFunction("to_json", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::function<MiniJson::Value(const Value&)> convert;
            convert = [&](const Value& v) -> MiniJson::Value {
                if (v.isDictionary()) {
                    MiniJson::Value mv(MiniJson::OBJECT);
                    for (const auto& kv : v.asDictionary().map) mv[kv.first] = convert(kv.second);
                    return mv;
                } else if (v.isArray()) {
                    MiniJson::Value mv(MiniJson::ARRAY);
                    for (const auto& item : v.asArray()) mv.append(convert(item));
                    return mv;
                } else if (v.isString()) return MiniJson::Value(v.asString());
                else if (v.isNumber()) {
                    double d = v.asNumber();
                    if (d == (int)d) return MiniJson::Value(std::to_string((int)d));
                    return MiniJson::Value(std::to_string(d));
                }
                else if (v.isBool()) return MiniJson::Value(v.asBool() ? "true" : "false");
                return MiniJson::Value("null");
            };
            MiniJson::Value root = convert(args[0]);
            std::stringstream ss;
            MiniJson::StreamWriter writer;
            writer.write(root, &ss);
            return Value(ss.str());
        }));

    // --- Terminal Built-ins ---

    // term_clear() - Clears the terminal screen (Windows)
    interp.defineGlobal("clear", Value::makeNativeFunction("clear", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            system("cls");
            return Value();
        }));

    // term_color(code) - Sets terminal text color (Windows)
    // 0-15: Standard Windows colors
    interp.defineGlobal("color", Value::makeNativeFunction("color", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) { interp.runtimeError("color() expects a number code (0-15)", 0, ""); return Value();
             }int code = (int)args[0].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, (WORD)code);
            return Value();
        }));

    // term_reset() - Resets terminal color to default
    interp.defineGlobal("reset", Value::makeNativeFunction("reset", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 7); // Default light gray/white
            return Value();
        }));

    // gotoxy(x, y) - Moves terminal cursor to coordinates
    interp.defineGlobal("gotoxy", Value::makeNativeFunction("gotoxy", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) 
                { interp.runtimeError("gotoxy() expects two numbers (x, y)", 0, ""); return Value();
             }int x = (int)args[0].asNumber();
            int y = (int)args[1].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(hConsole, pos);
            return Value();
        }));

    // getch() - Waits for and returns a single character
    interp.defineGlobal("getch", Value::makeNativeFunction("getch", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            int c = _getch();
            return Value(std::string(1, (char)c));
        }));

    // url_encode(str)
    static int curl_init_checker = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return 0; }();
    (void)curl_init_checker;
    interp.defineGlobal("url_encode", Value::makeNativeFunction("url_encode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string s = args[0].toString();
            CURL* curl = curl_easy_init();
            char* output = curl_easy_escape(curl, s.c_str(), (int)s.length());
            std::string res(output);
            curl_free(output);
            curl_easy_cleanup(curl);
            return Value(res);
        }));

    // url_decode(str)
    interp.defineGlobal("url_decode", Value::makeNativeFunction("url_decode", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            std::string s = args[0].toString();
            CURL* curl = curl_easy_init();
            int outlen;
            char* output = curl_easy_unescape(curl, s.c_str(), (int)s.length(), &outlen);
            std::string res(output, outlen);
            curl_free(output);
            curl_easy_cleanup(curl);
            return Value(res);
        }));

    // HTTP Helpers
    static auto HttpWriteCallback = [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    };

    // http_get(url, [headers])
    interp.defineGlobal("http_get", Value::makeNativeFunction("http_get", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.runtimeError("http_get() expects URL", 0, ""); return Value();
             }std::string url = args[0].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value();
             }std::string res;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            struct curl_slist* headers = nullptr;
            if (args.size() > 1 && args[1].isDictionary()) {
                for (auto& kv : args[1].asDictionary().map) {
                    std::string h = kv.first + ": " + kv.second.toString();
                    headers = curl_slist_append(headers, h.c_str());
                }
                curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            }
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_get failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value();
             }return Value(res);
        }));

    // http_post(url, body, [headers])
    interp.defineGlobal("http_post", Value::makeNativeFunction("http_post", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) { interp.runtimeError("http_post() expects URL and body", 0, ""); return Value();
             }std::string url = args[0].toString();
            std::string body = args[1].toString();
            CURL* curl = curl_easy_init();
            if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value();
             }std::string res;
            curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))HttpWriteCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &res);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L); // Bypass for local environments
            curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 second timeout
            
            struct curl_slist* headers = nullptr;
            bool hasCT = false;
            if (args.size() > 2 && args[2].isDictionary()) {
                for (auto& kv : args[2].asDictionary().map) {
                    std::string k = kv.first;
                    std::string h = k + ": " + kv.second.toString();
                    headers = curl_slist_append(headers, h.c_str());
                    if (k == "Content-Type") hasCT = true;
                }
            }
            if (!hasCT && !body.empty() && (body[0] == '{' || body[0] == '[')) {
                headers = curl_slist_append(headers, "Content-Type: application/json");
            }
            if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
            CURLcode code = curl_easy_perform(curl);
            if (headers) curl_slist_free_all(headers);
            curl_easy_cleanup(curl);
            if (code != CURLE_OK) { interp.runtimeError("http_post failed: " + std::string(curl_easy_strerror(code)), 0, ""); return Value();
             }return Value(res);
        }));

    // Database Aliases
    auto globalEnv = interp.getGlobalEnv();
    interp.defineGlobal("db_open", globalEnv->get("dbOpen", 0));
    interp.defineGlobal("db_execute", globalEnv->get("dbExec", 0));
    interp.defineGlobal("db_query", globalEnv->get("dbQuery", 0));
    interp.defineGlobal("db_close", globalEnv->get("dbClose", 0));
    interp.defineGlobal("db_last_insert_id", globalEnv->get("dbLastInsertId", 0));
    interp.defineGlobal("db_begin", globalEnv->get("dbBegin", 0));
    interp.defineGlobal("db_commit", globalEnv->get("dbCommit", 0));
    interp.defineGlobal("db_rollback", globalEnv->get("dbRollback", 0));
    // --- Async / Multithreading ---

    // spawn(fn, args...)
    interp.defineGlobal("spawn", Value::makeNativeFunction("spawn", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isCallable()) { interp.runtimeError("spawn() expects function", 0, ""); return Value();
            
             }Value func = args[0];
            std::vector<Value> fnArgs(args.begin() + 1, args.end());
            auto globalEnv = interp.getGlobalEnv();
            
            // Launch async task
            std::shared_future<Value> fut = std::async(std::launch::async, 
                [globalEnv, func, fnArgs]() -> Value {
                    Interpreter threadInterp;
                    threadInterp.setGlobalEnv(globalEnv);
                    return threadInterp.callFunction(func, fnArgs, 0, "native");
                }).share();
                
            return Value::makeFuture(fut);
        }));

    // await(future)
    auto awaitFn = [](Interpreter& interp, const std::vector<Value>& args) -> Value {
        if (!args[0].isFuture()) { interp.runtimeError("await() expects future", 0, ""); return Value();
         }auto fut = args[0].asFuture();
        fut->wait();
        return fut->get();
    };
    interp.defineGlobal("await", Value::makeNativeFunction("await", 1, awaitFn));
    interp.defineGlobal("sync", Value::makeNativeFunction("sync", 1, awaitFn));

    // fetch(url, [options])
    interp.defineGlobal("fetch", Value::makeNativeFunction("fetch", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (args.empty()) { interp.runtimeError("fetch() expects URL", 0, ""); return Value();
             }std::string url = args[0].toString();
            Value options;
            if (args.size() > 1) options = args[1];
            
            // Capture args by value for thread
            std::shared_future<Value> fut = std::async(std::launch::async, 
                [url, options, &interp]() -> Value {
                    CURL* curl = curl_easy_init();
                    if (!curl) { interp.runtimeError("CURL init failed", 0, ""); return Value();
                    
                     }std::string response;
                    std::string method = "GET";
                    std::string body;
                    struct curl_slist* headers = nullptr;
                    
                    if (options.isDictionary()) {
                        const auto& opts = options.asDictionary().map;
                        if (opts.count("method")) method = opts.at("method").toString();
                        if (opts.count("body")) body = opts.at("body").toString();
                        if (opts.count("headers") && opts.at("headers").isDictionary()) {
                            for (const auto& kv : opts.at("headers").asDictionary().map) {
                                std::string h = kv.first + ": " + kv.second.toString();
                                headers = curl_slist_append(headers, h.c_str());
                            }
                        }
                    }
                    
                    auto writeCb = [](void* contents, size_t size, size_t nmemb, void* userp) -> size_t {
                        ((std::string*)userp)->append((char*)contents, size * nmemb);
                        return size * nmemb;
                    };

                    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
                    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, (size_t(*)(void*,size_t,size_t,void*))writeCb);
                    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
                    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
                    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
                    
                    if (method == "POST") {
                        curl_easy_setopt(curl, CURLOPT_POST, 1L);
                        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
                    } else if (method != "GET") {
                        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());
                    }
                    
                    if (headers) curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
                    
                    CURLcode res = curl_easy_perform(curl);
                    if (headers) curl_slist_free_all(headers);
                    curl_easy_cleanup(curl);
                    
                    if (res != CURLE_OK) {
                         { interp.runtimeError("Fetch failed: " + std::string(curl_easy_strerror(res)), 0, ""); return Value();
                     }}
                    
                    return Value(response);
                }).share();
                
            return Value::makeFuture(fut);
        }));

    // --- PDF Built-ins ---
    interp.defineGlobal("pdf_begin", Value::makeNativeFunction("pdf_begin", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            g_pdf.begin(args[0].toString());
            return Value();
        }));

    interp.defineGlobal("pdf_add_page", Value::makeNativeFunction("pdf_add_page", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            g_pdf.addPage();
            return Value();
        }));

    interp.defineGlobal("pdf_text", Value::makeNativeFunction("pdf_text", 4,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            int size = (int)args[0].asNumber();
            int x = (int)args[1].asNumber();
            int y = (int)args[2].asNumber();
            std::string text = args[3].asString();
            g_pdf.currentStream << "BT /F1 " << size << " Tf " << x << " " << y << " Td (" << text << ") Tj ET\n";
            return Value();
        }));

    interp.defineGlobal("pdf_line", Value::makeNativeFunction("pdf_line", 4,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            int x1 = (int)args[0].asNumber();
            int y1 = (int)args[1].asNumber();
            int x2 = (int)args[2].asNumber();
            int y2 = (int)args[3].asNumber();
            g_pdf.currentStream << x1 << " " << y1 << " m " << x2 << " " << y2 << " l s\n";
            return Value();
        }));

    interp.defineGlobal("pdf_save", Value::makeNativeFunction("pdf_save", 0,
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            g_pdf.save();
            return Value();
        }));
}
