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

void registerBuiltins(Interpreter& interp) {
    // clock() - returns milliseconds since epoch
    interp.defineGlobal("clock", Value::makeNativeFunction("clock", 0,
        [](Interpreter&, const std::vector<Value>&) -> Value {
            auto now = std::chrono::system_clock::now();
            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()
            ).count();
            return Value((double)ms);
        }));

    // Input function
    interp.defineGlobal("__input__", Value::makeNativeFunction("input", 0, 
        [](Interpreter&, const std::vector<Value>&) -> Value {
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    // len(x) - length of string or array
    interp.defineGlobal("len", Value::makeNativeFunction("len", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                return Value(static_cast<double>(args[0].asString().length()));
            }
            if (args[0].isArray()) {
                return Value(static_cast<double>(args[0].asArray().size()));
            }
            if (args[0].isDictionary()) {
                return Value(static_cast<double>(args[0].asDictionary().map.size()));
            }
            throw RuntimeError("len() expects string or array");
        }));
    
    // push(arr, val) - add element to array
    interp.defineGlobal("push", Value::makeNativeFunction("push", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("push() expects array as first argument");
            }
            auto arr = args[0].asArrayPtr();
            arr->push_back(args[1]);
            return Value(arr);
        }));
    
    // pop(arr) - remove and return last element
    interp.defineGlobal("pop", Value::makeNativeFunction("pop", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("pop() expects array");
            }
            auto& arr = *args[0].asArrayPtr();
            if (arr.empty()) {
                throw RuntimeError("pop() on empty array");
            }
            Value last = arr.back();
            arr.pop_back();
            return last;
        }));
    
    // str(x) - convert to string
    interp.defineGlobal("str", Value::makeNativeFunction("str", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            return Value(args[0].toString());
        }));
    
    // num(x) - convert to number
    interp.defineGlobal("num", Value::makeNativeFunction("num", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args[0].isNumber()) return args[0];
            if (args[0].isString()) {
                try {
                    return Value(std::stod(args[0].asString()));
                } catch (...) {
                    throw RuntimeError("Cannot convert '" + args[0].asString() + "' to number");
                }
            }
            if (args[0].isBool()) {
                return Value(args[0].asBool() ? 1.0 : 0.0);
            }
            throw RuntimeError("Cannot convert " + args[0].typeName() + " to number");
        }));
    
    // type(x) - get type name
    interp.defineGlobal("type", Value::makeNativeFunction("type", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            return Value(args[0].typeName());
        }));
    
    // substr(s, start, len) - get substring
    interp.defineGlobal("substr", Value::makeNativeFunction("substr", 3,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("substr() expects string as first argument");
            }
            if (!args[1].isNumber() || !args[2].isNumber()) {
                throw RuntimeError("substr() expects numbers for start and length");
            }
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) {
                throw RuntimeError("split() expects two strings");
            }
            
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("join() expects array as first argument");
            }
            if (!args[1].isString()) {
                throw RuntimeError("join() expects string as delimiter");
            }
            
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("floor() expects number");
            }
            return Value(std::floor(args[0].asNumber()));
        }));
    
    // ceil(x)
    interp.defineGlobal("ceil", Value::makeNativeFunction("ceil", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("ceil() expects number");
            }
            return Value(std::ceil(args[0].asNumber()));
        }));
    
    // abs(x)
    interp.defineGlobal("abs", Value::makeNativeFunction("abs", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("abs() expects number");
            }
            return Value(std::abs(args[0].asNumber()));
        }));
    
    // sqrt(x)
    interp.defineGlobal("sqrt", Value::makeNativeFunction("sqrt", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("sqrt() expects number");
            }
            double val = args[0].asNumber();
            if (val < 0) {
                throw RuntimeError("sqrt() of negative number");
            }
            return Value(std::sqrt(val));
        }));
    
    // pow(base, exp)
    interp.defineGlobal("pow", Value::makeNativeFunction("pow", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                throw RuntimeError("pow() expects two numbers");
            }
            return Value(std::pow(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // rand() - random number 0-1
    interp.defineGlobal("rand", Value::makeNativeFunction("rand", 0,
        [](Interpreter&, const std::vector<Value>&) -> Value {
            return Value(static_cast<double>(std::rand()) / RAND_MAX);
        }));
    
    // randint(min, max) - random integer in range
    interp.defineGlobal("randint", Value::makeNativeFunction("randint", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                throw RuntimeError("randint() expects two numbers");
            }
            int min = static_cast<int>(args[0].asNumber());
            int max = static_cast<int>(args[1].asNumber());
            return Value(static_cast<double>(min + std::rand() % (max - min + 1)));
        }));
    
    // round(x)
    interp.defineGlobal("round", Value::makeNativeFunction("round", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) {
                throw RuntimeError("round() expects number");
            }
            return Value(std::round(args[0].asNumber()));
        }));
    
    // min(a, b)
    interp.defineGlobal("min", Value::makeNativeFunction("min", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                throw RuntimeError("min() expects two numbers");
            }
            return Value(std::min(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // max(a, b)
    interp.defineGlobal("max", Value::makeNativeFunction("max", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) {
                throw RuntimeError("max() expects two numbers");
            }
            return Value(std::max(args[0].asNumber(), args[1].asNumber()));
        }));
    
    // contains(str/arr, item) - check if string/array contains item
    interp.defineGlobal("contains", Value::makeNativeFunction("contains", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                if (!args[1].isString()) {
                    throw RuntimeError("contains() with string expects string to search for");
                }
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
            throw RuntimeError("contains() expects string, array, or dictionary");
        }));
    
    // indexOf(str/arr, item) - find index of item
    interp.defineGlobal("indexOf", Value::makeNativeFunction("indexOf", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args[0].isString()) {
                if (!args[1].isString()) {
                    throw RuntimeError("indexOf() with string expects string to search for");
                }
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
            throw RuntimeError("indexOf() expects string or array");
        }));
    
    // reverse(arr/str) - reverse array or string
    interp.defineGlobal("reverse", Value::makeNativeFunction("reverse", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
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
            throw RuntimeError("reverse() expects string or array");
        }));
    
    // sort(arr) - sort array
    interp.defineGlobal("sort", Value::makeNativeFunction("sort", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("sort() expects array");
            }
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("upper() expects string");
            }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::toupper);
            return Value(s);
        }));
    
    // lower(str) - lowercase
    interp.defineGlobal("lower", Value::makeNativeFunction("lower", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("lower() expects string");
            }
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(), ::tolower);
            return Value(s);
        }));
    
    // trim(str) - trim whitespace
    interp.defineGlobal("trim", Value::makeNativeFunction("trim", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("trim() expects string");
            }
            std::string s = args[0].asString();
            s.erase(0, s.find_first_not_of(" \t\n\r"));
            s.erase(s.find_last_not_of(" \t\n\r") + 1);
            return Value(s);
        }));
    
    // replace(str, old, new) - replace substring
    interp.defineGlobal("replace", Value::makeNativeFunction("replace", 3,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString() || !args[2].isString()) {
                throw RuntimeError("replace() expects three strings");
            }
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
    
    // slice(arr/str, start, end) - get slice
    interp.defineGlobal("slice", Value::makeNativeFunction("slice", 3,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[1].isNumber() || !args[2].isNumber()) {
                throw RuntimeError("slice() expects numbers for start and end");
            }
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
            throw RuntimeError("slice() expects string or array");
        }));
    
    // print (alias for out but as function)
    interp.defineGlobal("print", Value::makeNativeFunction("print", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value();
        }));
    
    // input(prompt) - input with optional prompt
    interp.defineGlobal("input", Value::makeNativeFunction("input", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    // range(end) or range(start, end) - create array from range
    interp.defineGlobal("range", Value::makeNativeFunction("range", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.empty() || args.size() > 2) {
                throw RuntimeError("range() expects 1 or 2 arguments");
            }
            
            int start = 0, end = 0;
            if (args.size() == 1) {
                if (!args[0].isNumber()) throw RuntimeError("range() expects number");
                end = static_cast<int>(args[0].asNumber());
            } else {
                if (!args[0].isNumber() || !args[1].isNumber()) {
                    throw RuntimeError("range() expects numbers");
                }
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
                throw RuntimeError("map() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("map() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            
            for (const auto& elem : arr) {
                result.push_back(interp.callFunction(args[1], {elem}, 0));
            }
            
            return Value::makeArray(result);
        }));
    
    // filter(arr, fn) - filter elements where fn returns truthy
    interp.defineGlobal("filter", Value::makeNativeFunction("filter", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("filter() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("filter() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            std::vector<Value> result;
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0);
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
                throw RuntimeError("reduce() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("reduce() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            Value acc = args[2];
            
            for (const auto& elem : arr) {
                acc = interp.callFunction(args[1], {acc, elem}, 0);
            }
            
            return acc;
        }));
    
    // forEach(arr, fn) - apply function to each element (no return)
    interp.defineGlobal("forEach", Value::makeNativeFunction("forEach", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("forEach() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("forEach() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                interp.callFunction(args[1], {elem}, 0);
            }
            
            return Value();
        }));
    
    // find(arr, fn) - find first element where fn returns truthy
    interp.defineGlobal("find", Value::makeNativeFunction("find", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) {
                throw RuntimeError("find() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("find() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0);
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
                throw RuntimeError("every() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("every() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0);
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
                throw RuntimeError("some() expects array as first argument");
            }
            if (!args[1].isCallable()) {
                throw RuntimeError("some() expects function as second argument");
            }
            
            const auto& arr = args[0].asArray();
            
            for (const auto& elem : arr) {
                Value test = interp.callFunction(args[1], {elem}, 0);
                if (test.isTruthy()) {
                    return Value(true);
                }
            }
            
            return Value(false);
        }));
    
    // readFile(path) - read file content as string
    interp.defineGlobal("readFile", Value::makeNativeFunction("readFile", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("readFile() expects string path");
            }
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "'");
            }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return Value(buffer.str());
        }));
    
    // writeFile(path, content) - write string to file
    interp.defineGlobal("writeFile", Value::makeNativeFunction("writeFile", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("writeFile() expects string path");
            }
            if (!args[1].isString()) {
                throw RuntimeError("writeFile() expects string content");
            }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "' for writing");
            }
            file << content;
            return Value(true);
        }));
    
    // appendFile(path, content) - append string to file
    interp.defineGlobal("appendFile", Value::makeNativeFunction("appendFile", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("appendFile() expects string path");
            }
            if (!args[1].isString()) {
                throw RuntimeError("appendFile() expects string content");
            }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "' for appending");
            }
            file << content;
            return Value(true);
        }));
    
    // readLines(path) - read file into array of lines
    interp.defineGlobal("readLines", Value::makeNativeFunction("readLines", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("readLines() expects string path");
            }
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "'");
            }
            std::vector<Value> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(Value(line));
            }
            return Value::makeArray(lines);
        }));
    
    // writeLine(path, content) - write string with newline to file
    interp.defineGlobal("writeLine", Value::makeNativeFunction("writeLine", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("writeLine() expects string path");
            }
            if (!args[1].isString()) {
                throw RuntimeError("writeLine() expects string content");
            }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "' for writing");
            }
            file << content << std::endl;
            return Value(true);
        }));
    
    // appendLine(path, content) - append string with newline to file
    interp.defineGlobal("appendLine", Value::makeNativeFunction("appendLine", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) {
                throw RuntimeError("appendLine() expects string path");
            }
            if (!args[1].isString()) {
                throw RuntimeError("appendLine() expects string content");
            }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) {
                throw RuntimeError("Could not open file '" + path + "' for appending");
            }
            file << content << std::endl;
            return Value(true);
        }));
    
    // keys(dict)
    interp.defineGlobal("keys", Value::makeNativeFunction("keys", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) throw RuntimeError("keys() expects dictionary");
            const auto& map = args[0].asDictionary().map;
            std::vector<Value> keys;
            for (const auto& kv : map) {
                keys.push_back(Value(kv.first));
            }
            return Value::makeArray(keys);
        }));
    
    // values(dict)
    interp.defineGlobal("values", Value::makeNativeFunction("values", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) throw RuntimeError("values() expects dictionary");
            const auto& map = args[0].asDictionary().map;
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
            if (!args[0].isNumber()) throw RuntimeError("server() port must be a number");
            if (!args[1].isFunction()) throw RuntimeError("server() handler must be a function");
            
            int port = static_cast<int>(args[0].asNumber());
            Value handler = args[1];

            WSADATA wsaData;
            if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
                throw RuntimeError("WSAStartup failed");
            }

            SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
            if (listenSocket == INVALID_SOCKET) {
                WSACleanup();
                throw RuntimeError("Socket creation failed");
            }

            sockaddr_in serverAddr;
            serverAddr.sin_family = AF_INET;
            serverAddr.sin_addr.s_addr = INADDR_ANY;
            serverAddr.sin_port = htons(port);

            if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
                closesocket(listenSocket);
                WSACleanup();
                throw RuntimeError("Bind failed");
            }

            if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
                closesocket(listenSocket);
                WSACleanup();
                throw RuntimeError("Listen failed");
            }

            while (true) {
                SOCKET clientSocket = accept(listenSocket, nullptr, nullptr);
                if (clientSocket == INVALID_SOCKET) break;

                char recvbuf[8192]; // Increased buffer
                int recvResult = recv(clientSocket, recvbuf, sizeof(recvbuf) - 1, 0);
                if (recvResult > 0) {
                    recvbuf[recvResult] = '\0';
                    std::string request(recvbuf);
                    
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
                            size_t lineEnd = request.find("\r\n");
                            if (lineEnd != std::string::npos) {
                                version = request.substr(secondSpace + 1, lineEnd - (secondSpace + 1));
                            }
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

                    // Parse Headers
                    size_t headStart = request.find("\r\n") + 2;
                    size_t bodyStart = request.find("\r\n\r\n");
                    if (bodyStart != std::string::npos) {
                        std::string headStr = request.substr(headStart, bodyStart - headStart);
                        body = request.substr(bodyStart + 4);
                        
                        size_t pos = 0;
                        while (pos < headStr.length()) {
                            size_t nextLine = headStr.find("\r\n", pos);
                            std::string line = headStr.substr(pos, nextLine == std::string::npos ? nextLine : nextLine - pos);
                            size_t colon = line.find(':');
                            if (colon != std::string::npos) {
                                std::string k = line.substr(0, colon);
                                std::string v = line.substr(colon + 1);
                                // Trim v
                                v.erase(0, v.find_first_not_of(" "));
                                headers[k] = Value(v);
                            }
                            if (nextLine == std::string::npos) break;
                            pos = nextLine + 2;
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

                    Value queryDict = Value::makeDictionary();
                    queryDict.asDictionary().map = std::move(query);
                    reqMap["query"] = queryDict;

                    Value headerDict = Value::makeDictionary();
                    headerDict.asDictionary().map = std::move(headers);
                    reqMap["headers"] = headerDict;

                    std::vector<Value> callbackArgs = {reqArg};
                    try {
                        Value result = interp.callFunction(handler, callbackArgs, 0);
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
            }

            closesocket(listenSocket);
            WSACleanup();
            return Value();
        }));
#else
    interp.defineGlobal("server", Value::makeNativeFunction("server", 2,
        [](Interpreter&, const std::vector<Value>&) -> Value {
            throw RuntimeError("server() is only supported on Windows");
        }));
#endif

    // serveFile(path) - helper to serve a file with correct headers
    interp.defineGlobal("serveFile", Value::makeNativeFunction("serveFile", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("serveFile() expects string path");
            std::string path = args[0].asString();
            
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("dbOpen() expects string path");
            std::string path = args[0].asString();
            
            sqlite3* db;
            int rc = sqlite3_open(path.c_str(), &db);
            if (rc != SQLITE_OK) {
                std::string err = sqlite3_errmsg(db);
                sqlite3_close(db);
                throw RuntimeError("sqlite3_open failed: " + err);
            }
            
            int handle = nextDbHandle++;
            dbConnections[handle] = db;
            return Value((double)handle);
        }));

    // dbExec(handle, sql, [params])
    interp.defineGlobal("dbExec", Value::makeNativeFunction("dbExec", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) throw RuntimeError("dbExec() expects at least 2 arguments");
            if (!args[0].isNumber()) throw RuntimeError("dbExec() expects number handle");
            if (!args[1].isString()) throw RuntimeError("dbExec() expects string SQL");
            
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                throw RuntimeError("Invalid database handle");
            }
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw RuntimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)));
            }
            
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
                throw RuntimeError("sqlite3_step failed: " + std::string(sqlite3_errmsg(db)));
            }
            
            return Value(true);
        }));

    // dbQuery(handle, sql, [params])
    interp.defineGlobal("dbQuery", Value::makeNativeFunction("dbQuery", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) throw RuntimeError("dbQuery() expects at least 2 arguments");
            if (!args[0].isNumber()) throw RuntimeError("dbQuery() expects number handle");
            if (!args[1].isString()) throw RuntimeError("dbQuery() expects string SQL");
            
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) {
                throw RuntimeError("Invalid database handle");
            }
            
            sqlite3* db = dbConnections[handle];
            sqlite3_stmt* stmt;
            if (sqlite3_prepare_v2(db, args[1].asString().c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
                throw RuntimeError("sqlite3_prepare_v2 failed: " + std::string(sqlite3_errmsg(db)));
            }
            
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("dbClose() expects number handle");
            
            int handle = (int)args[0].asNumber();
            auto it = dbConnections.find(handle);
            if (it != dbConnections.end()) {
                sqlite3_close(it->second);
                dbConnections.erase(it);
            }
            return Value(true);
        }));

    // dbLastInsertId(handle)
    interp.defineGlobal("dbLastInsertId", Value::makeNativeFunction("dbLastInsertId", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("dbLastInsertId() expects number handle");
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) throw RuntimeError("Invalid database handle");
            return Value((double)sqlite3_last_insert_rowid(dbConnections[handle]));
        }));

    // dbBegin(handle)
    interp.defineGlobal("dbBegin", Value::makeNativeFunction("dbBegin", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("dbBegin() expects number handle");
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) throw RuntimeError("Invalid database handle");
            sqlite3_exec(dbConnections[handle], "BEGIN TRANSACTION", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // dbCommit(handle)
    interp.defineGlobal("dbCommit", Value::makeNativeFunction("dbCommit", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("dbCommit() expects number handle");
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) throw RuntimeError("Invalid database handle");
            sqlite3_exec(dbConnections[handle], "COMMIT", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // dbRollback(handle)
    interp.defineGlobal("dbRollback", Value::makeNativeFunction("dbRollback", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("dbRollback() expects number handle");
            int handle = (int)args[0].asNumber();
            if (dbConnections.find(handle) == dbConnections.end()) throw RuntimeError("Invalid database handle");
            sqlite3_exec(dbConnections[handle], "ROLLBACK", nullptr, nullptr, nullptr);
            return Value(true);
        }));

    // ord(str) - returns ASCII value of first char
    interp.defineGlobal("ord", Value::makeNativeFunction("ord", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("ord() expects string");
            std::string s = args[0].asString();
            if (s.empty()) return Value(0.0);
            return Value((double)(unsigned char)s[0]);
        }));

    // chr(num) - returns char from ASCII value
    interp.defineGlobal("chr", Value::makeNativeFunction("chr", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("chr() expects number");
            char c = (char)(int)args[0].asNumber();
            return Value(std::string(1, c));
        }));

    // xor(a, b) - bitwise XOR
    interp.defineGlobal("xor", Value::makeNativeFunction("xor", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) throw RuntimeError("xor() expects numbers");
            int a = (int)args[0].asNumber();
            int b = (int)args[1].asNumber();
            return Value((double)(a ^ b));
        }));

    // substring(str, start, [len])
    interp.defineGlobal("substring", Value::makeNativeFunction("substring", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || args.size() > 3) throw RuntimeError("substring() expects 2 or 3 arguments");
            if (!args[0].isString()) throw RuntimeError("substring() first arg must be string");
            if (!args[1].isNumber()) throw RuntimeError("substring() start must be number");
            
            std::string s = args[0].asString();
            int start = (int)args[1].asNumber();
            int len = (args.size() == 3 && args[2].isNumber()) ? (int)args[2].asNumber() : (int)s.length() - start;
            
            if (start < 0) start = 0;
            if (start > (int)s.length()) return Value("");
            if (len < 0) len = 0;
            
            return Value(s.substr(start, len));
        }));

    // split(str, delimiter)
    interp.defineGlobal("split", Value::makeNativeFunction("split", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString() || !args[1].isString()) throw RuntimeError("split() expects strings");
            std::string s = args[0].asString();
            std::string delim = args[1].asString();
            std::vector<Value> results;
            
            if (delim.empty()) {
                for (char c : s) results.push_back(Value(std::string(1, c)));
            } else {
                size_t start = 0;
                size_t end = s.find(delim);
                while (end != std::string::npos) {
                    results.push_back(Value(s.substr(start, end - start)));
                    start = end + delim.length();
                    end = s.find(delim, start);
                }
                results.push_back(Value(s.substr(start)));
            }
            return Value::makeArray(results);
        }));

    // join(array, delimiter)
    interp.defineGlobal("join", Value::makeNativeFunction("join", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) throw RuntimeError("join() expects array as first arg");
            if (!args[1].isString()) throw RuntimeError("join() expects string delimiter");
            
            const auto& arr = args[0].asArray();
            std::string delim = args[1].asString();
            std::string result = "";
            for (size_t i = 0; i < arr.size(); i++) {
                if (i > 0) result += delim;
                result += arr[i].toString();
            }
            return Value(result);
        }));

    // push(array, value)
    interp.defineGlobal("push", Value::makeNativeFunction("push", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) throw RuntimeError("push() expects array");
            args[0].asArrayPtr()->push_back(args[1]);
            return args[1];
        }));

    // pop(array)
    interp.defineGlobal("pop", Value::makeNativeFunction("pop", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isArray()) throw RuntimeError("pop() expects array");
            auto arrPtr = args[0].asArrayPtr();
            if (arrPtr->empty()) return Value();
            Value val = arrPtr->back();
            arrPtr->pop_back();
            return val;
        }));

    // toLower(str)
    interp.defineGlobal("toLower", Value::makeNativeFunction("toLower", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("toLower() expects string");
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    // toUpper(str)
    interp.defineGlobal("toUpper", Value::makeNativeFunction("toUpper", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("toUpper() expects string");
            std::string s = args[0].asString();
            for (auto& c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
            return Value(s);
        }));

    // typeOf(val)
    interp.defineGlobal("typeOf", Value::makeNativeFunction("typeOf", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            return Value(args[0].typeName());
        }));

    // dictRemove(dict, key)
    interp.defineGlobal("dictRemove", Value::makeNativeFunction("dictRemove", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isDictionary()) throw RuntimeError("dictRemove() expects dictionary");
            std::string key = args[1].toString();
            args[0].asDictionaryPtr()->map.erase(key);
            return args[0];
        }));

    // --- Added Missing Functions ---

    // stop(ms) - Sleep for specified milliseconds
    interp.defineGlobal("stop", Value::makeNativeFunction("stop", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("stop() expects number");
            std::this_thread::sleep_for(std::chrono::milliseconds((int)args[0].asNumber()));
            return Value();
        }));

    // parse_json(str) - Convert JSON string to EZ value
    interp.defineGlobal("parse_json", Value::makeNativeFunction("parse_json", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) throw RuntimeError("parse_json() expects string");
            MiniJson::Value root;
            MiniJson::Reader reader;
            if (!reader.parse(args[0].asString(), root)) {
                throw RuntimeError("Failed to parse JSON");
            }
            
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
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
        [](Interpreter&, const std::vector<Value>&) -> Value {
            system("cls");
            return Value();
        }));

    // term_color(code) - Sets terminal text color (Windows)
    // 0-15: Standard Windows colors
    interp.defineGlobal("color", Value::makeNativeFunction("color", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber()) throw RuntimeError("color() expects a number code (0-15)");
            int code = (int)args[0].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, (WORD)code);
            return Value();
        }));

    // term_reset() - Resets terminal color to default
    interp.defineGlobal("reset", Value::makeNativeFunction("reset", 0,
        [](Interpreter&, const std::vector<Value>&) -> Value {
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            SetConsoleTextAttribute(hConsole, 7); // Default light gray/white
            return Value();
        }));

    // gotoxy(x, y) - Moves terminal cursor to coordinates
    interp.defineGlobal("gotoxy", Value::makeNativeFunction("gotoxy", 2,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (!args[0].isNumber() || !args[1].isNumber()) 
                throw RuntimeError("gotoxy() expects two numbers (x, y)");
            int x = (int)args[0].asNumber();
            int y = (int)args[1].asNumber();
            HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
            COORD pos = { (SHORT)x, (SHORT)y };
            SetConsoleCursorPosition(hConsole, pos);
            return Value();
        }));

    // getch() - Waits for and returns a single character
    interp.defineGlobal("getch", Value::makeNativeFunction("getch", 0,
        [](Interpreter&, const std::vector<Value>&) -> Value {
            int c = _getch();
            return Value(std::string(1, (char)c));
        }));

    // url_encode(str)
    static int curl_init_checker = []() { curl_global_init(CURL_GLOBAL_DEFAULT); return 0; }();
    (void)curl_init_checker;
    interp.defineGlobal("url_encode", Value::makeNativeFunction("url_encode", 1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
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
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.empty()) throw RuntimeError("http_get() expects URL");
            std::string url = args[0].toString();
            CURL* curl = curl_easy_init();
            if (!curl) throw RuntimeError("CURL init failed");
            std::string res;
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
            if (code != CURLE_OK) throw RuntimeError("http_get failed: " + std::string(curl_easy_strerror(code)));
            return Value(res);
        }));

    // http_post(url, body, [headers])
    interp.defineGlobal("http_post", Value::makeNativeFunction("http_post", -1,
        [](Interpreter&, const std::vector<Value>& args) -> Value {
            if (args.size() < 2) throw RuntimeError("http_post() expects URL and body");
            std::string url = args[0].toString();
            std::string body = args[1].toString();
            CURL* curl = curl_easy_init();
            if (!curl) throw RuntimeError("CURL init failed");
            std::string res;
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
            if (code != CURLE_OK) throw RuntimeError("http_post failed: " + std::string(curl_easy_strerror(code)));
            return Value(res);
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
}
