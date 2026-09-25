#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/Utf8.h"
#include "eventloop/EventLoop.h"
#include "gc/CycleCollector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "runtime/SecurityPolicy.h"
#include "gc/CycleCollector.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <system_error>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

// ── File stream storage ────────────────────────────────────────────────────────
// A Value cannot carry an fstream, so the handles live in a side table keyed by
// the owning EZInstance. Every access goes through the helpers below, which hold
// a mutex.
//
// The table used to be pruned ONLY by File.close(), which made two problems:
//
//   1. A file that was never closed stayed in the table forever. The table holds
//      a strong reference, so the fstream was never destroyed and the OS handle
//      never released -- 400 unclosed files kept their path locked against
//      deletion, and a long-running process would exhaust its handle budget.
//      The old comment claimed the shared_ptr destructor was a safety net; it
//      could not be, because this very table was the reference keeping it alive.
//
//   2. The key is a raw pointer that was never invalidated. Once an instance was
//      freed its entry became a stale mapping from an address the allocator is
//      free to hand out again, so a NEW file could have inherited the dead
//      one's stream.
//
// Both are fixed by remembering the owner weakly and dropping entries whose
// owner has died. Erasing the entry releases the last reference to the fstream,


void registerIOBuiltins(RuntimeContext& interp) {

    interp.defineGlobal("__input__", Value::makeNativeFunction("input", 0, 
        [](RuntimeContext& interp, const std::vector<Value>&) -> Value {
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));
    
    interp.defineGlobal("input", Value::makeNativeFunction("input", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args.empty()) {
                std::cout << args[0].toString();
            }
            std::string line;
            std::getline(std::cin, line);
            return Value(line);
        }));

    interp.defineGlobal("print", Value::makeNativeFunction("print", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::endl;
            return Value();
        }));

    // write(...) — like print, but WITHOUT a trailing newline.
    //
    // This is what makes in-place terminal output possible: a progress bar or
    // spinner writes its line, emits a carriage return, and overwrites itself
    // on the next update. print() cannot do that, because every call ends the
    // line.
    //
    // The explicit flush is the part that is easy to miss. std::endl flushes
    // as a side effect, so print() always appears immediately; without endl
    // the text sits in the buffer and an animation shows nothing at all until
    // the program exits, at which point it dumps every frame at once.
    interp.defineGlobal("write", Value::makeNativeFunction("write", -1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) std::cout << " ";
                std::cout << args[i].toString();
            }
            std::cout << std::flush;
            return Value();
        }));

    interp.defineGlobal("readFile", Value::makeNativeFunction("readFile", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "readFile() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            if (!SecurityPolicy::checkRead(interp, path)) return Value();
            std::ifstream file(path, std::ios::binary);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "'", 0, ""); return Value(); }
            std::stringstream buffer;
            buffer << file.rdbuf();
            return Value(buffer.str());
        }));
    
    interp.defineGlobal("writeFile", Value::makeNativeFunction("writeFile", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "writeFile() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.throwException("TypeError", "writeFile() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            if (!SecurityPolicy::checkWrite(interp, path)) return Value();
            
            std::ofstream file(path, std::ios::binary);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for writing", 0, ""); return Value(); }
            file.write(content.data(), content.size());
            return Value(true);
        }));

    interp.defineGlobal("appendFile", Value::makeNativeFunction("appendFile", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "appendFile() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.throwException("TypeError", "appendFile() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            if (!SecurityPolicy::checkWrite(interp, path)) return Value();
            
            std::ofstream file(path, std::ios::app | std::ios::binary);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for appending", 0, ""); return Value(); }
            file.write(content.data(), content.size());
            return Value(true);
        }));

    interp.defineGlobal("readLines", Value::makeNativeFunction("readLines", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "readLines() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            if (!SecurityPolicy::checkRead(interp, path)) return Value();
            std::ifstream file(path);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "'", 0, ""); return Value(); }
            std::vector<Value> lines;
            std::string line;
            while (std::getline(file, line)) {
                lines.push_back(Value(line));
            }
            return Value::makeArray(lines);
        }));
    
    interp.defineGlobal("writeLine", Value::makeNativeFunction("writeLine", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "writeLine() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.throwException("TypeError", "writeLine() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            if (!SecurityPolicy::checkWrite(interp, path)) return Value();
            
            std::ofstream file(path);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for writing", 0, ""); return Value(); }
            file << content << std::endl;
            return Value(true);
        }));
    
    interp.defineGlobal("appendLine", Value::makeNativeFunction("appendLine", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "appendLine() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.throwException("TypeError", "appendLine() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            if (!SecurityPolicy::checkWrite(interp, path)) return Value();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for appending", 0, ""); return Value(); }
            file << content << std::endl;
            return Value(true);
        }));

    // ── File class ─────────────────────────────────────────────────────────────
    // Streaming file I/O class that avoids loading entire files into memory.
    //
    // Usage:
    //   f = File("data.txt", "r")
    //   line = f.readLine()
    //   f.close()

    }
