#include "../Builtins.h"
#include "../Interpreter.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>

void registerIOBuiltins(Interpreter& interp) {
    interp.defineGlobal("__input__", Value::makeNativeFunction("input", 0, 
        [](Interpreter& interp, const std::vector<Value>&) -> Value {
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    interp.defineGlobal("input", Value::makeNativeFunction("input", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));

    interp.defineGlobal("print", Value::makeNativeFunction("print", -1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value();
        }));

    interp.defineGlobal("readFile", Value::makeNativeFunction("readFile", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("readFile() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "'", 0, ""); return Value(); }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return Value(buffer.str());
        }));
    
    interp.defineGlobal("writeFile", Value::makeNativeFunction("writeFile", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("writeFile() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("writeFile() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "' for writing", 0, ""); return Value(); }
            file << content;
            return Value(true);
        }));
    
    interp.defineGlobal("appendFile", Value::makeNativeFunction("appendFile", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("appendFile() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("appendFile() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "' for appending", 0, ""); return Value(); }
            file << content;
            return Value(true);
        }));
    
    interp.defineGlobal("readLines", Value::makeNativeFunction("readLines", 1,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("readLines() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::ifstream file(path);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "'", 0, ""); return Value(); }
            std::vector<Value> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(Value(line));
            }
            return Value::makeArray(lines);
        }));
    
    interp.defineGlobal("writeLine", Value::makeNativeFunction("writeLine", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("writeLine() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("writeLine() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "' for writing", 0, ""); return Value(); }
            file << content << std::endl;
            return Value(true);
        }));
    
    interp.defineGlobal("appendLine", Value::makeNativeFunction("appendLine", 2,
        [](Interpreter& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.runtimeError("appendLine() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.runtimeError("appendLine() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) { interp.runtimeError("Could not open file '" + path + "' for appending", 0, ""); return Value(); }
            file << content << std::endl;
            return Value(true);
        }));
}
