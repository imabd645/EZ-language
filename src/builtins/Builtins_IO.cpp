#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
#include "gc/CycleCollector.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <unordered_map>
#include <mutex>
#include <memory>

// ── File stream storage ────────────────────────────────────────────────────────
// We cannot add a new variant to Value without modifying Value.h, so we store
// the fstream handles in a global map keyed by EZInstance raw pointer.  Every
// access goes through the helpers below which hold a mutex.
// When an instance is garbage-collected its destructor will NOT automatically
// close the stream, so File.close() is the canonical cleanup path.  As a safety
// net the shared_ptr destructor will close the fstream when refcount hits zero.

static std::mutex                                                     g_fileMtx;
static std::unordered_map<EZInstance*, std::shared_ptr<std::fstream>>  g_fileStreams;

static void storeStream(EZInstance* inst, std::shared_ptr<std::fstream> fs) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    g_fileStreams[inst] = std::move(fs);
}

static std::shared_ptr<std::fstream> getStream(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    auto it = g_fileStreams.find(inst);
    if (it != g_fileStreams.end()) return it->second;
    return nullptr;
}

static void removeStream(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    g_fileStreams.erase(inst);
}

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

    interp.defineGlobal("readFile", Value::makeNativeFunction("readFile", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "readFile() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::ifstream file(path);
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
            
            std::ofstream file(path);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for writing", 0, ""); return Value(); }
            file << content;
            return Value(true);
        }));
    
    interp.defineGlobal("appendFile", Value::makeNativeFunction("appendFile", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "appendFile() expects string path", 0, ""); return Value(); }
            if (!args[1].isString()) { interp.throwException("TypeError", "appendFile() expects string content", 0, ""); return Value(); }
            std::string path = args[0].asString();
            std::string content = args[1].asString();
            
            std::ofstream file(path, std::ios::app);
            if (!file.is_open()) { interp.throwException("FileNotFoundError", "Could not open file '" + path + "' for appending", 0, ""); return Value(); }
            file << content;
            return Value(true);
        }));
    
    interp.defineGlobal("readLines", Value::makeNativeFunction("readLines", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (!args[0].isString()) { interp.throwException("TypeError", "readLines() expects string path", 0, ""); return Value(); }
            std::string path = args[0].asString();
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

    auto fileClass = std::make_shared<EZClass>("File");
    CycleCollector::instance().track(fileClass, ValueType::CLASS);

    // File.init(path, mode)
    // mode: "r" = read, "w" = write, "a" = append
    //       "rb"/"wb"/"ab" = binary variants
    fileClass->setMethod("init", Value::makeNativeFunction("init", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            if (!args[1].isString()) {
                interp.throwException("TypeError", "File() expects string path as first argument", 0, "");
                return Value();
            }
            if (!args[2].isString()) {
                interp.throwException("TypeError", "File() expects string mode as second argument", 0, "");
                return Value();
            }

            std::string path = args[1].asString();
            std::string mode = args[2].asString();

            std::ios::openmode flags = static_cast<std::ios::openmode>(0);
            if (mode == "r") {
                flags = std::ios::in;
            } else if (mode == "w") {
                flags = std::ios::out | std::ios::trunc;
            } else if (mode == "a") {
                flags = std::ios::out | std::ios::app;
            } else if (mode == "rb") {
                flags = std::ios::in | std::ios::binary;
            } else if (mode == "wb") {
                flags = std::ios::out | std::ios::trunc | std::ios::binary;
            } else if (mode == "ab") {
                flags = std::ios::out | std::ios::app | std::ios::binary;
            } else if (mode == "rw") {
                flags = std::ios::in | std::ios::out;
            } else {
                interp.throwException("ValueError",
                    "File() invalid mode '" + mode + "'. Expected 'r','w','a','rb','wb','ab', or 'rw'", 0, "");
                return Value();
            }

            auto fs = std::make_shared<std::fstream>(path, flags);
            if (!fs->is_open()) {
                interp.throwException("FileNotFoundError",
                    "Could not open file '" + path + "' with mode '" + mode + "'", 0, "");
                return Value();
            }

            // Store the stream handle and metadata on the instance
            storeStream(instance.get(), fs);
            instance->setProperty("_path", Value(path));
            instance->setProperty("_mode", Value(mode));
            instance->setProperty("_open", Value(true));
            return args[0];
        }));

    // File.readLine() -> string | nil
    // Reads one line (without trailing newline).  Returns nil at EOF.
    fileClass->setMethod("readLine", Value::makeNativeFunction("readLine", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "readLine() called on a closed File", 0, "");
                return Value();
            }
            std::string line;
            if (std::getline(*fs, line)) {
                return Value(line);
            }
            return Value(); // nil at EOF
        }));

    // File.read(n) -> string
    // Reads up to n bytes.  Returns empty string at EOF.
    fileClass->setMethod("read", Value::makeNativeFunction("read", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "read() called on a closed File", 0, "");
                return Value();
            }
            if (!args[1].isNumber()) {
                interp.throwException("TypeError", "File.read() expects integer byte count", 0, "");
                return Value();
            }
            size_t n = static_cast<size_t>(args[1].asNumber());
            if (n == 0) return Value(std::string(""));

            std::string buf(n, '\0');
            fs->read(&buf[0], n);
            auto bytesRead = fs->gcount();
            buf.resize(static_cast<size_t>(bytesRead));
            return Value(buf);
        }));

    // File.readAll() -> string
    // Reads the entire remaining content.  Use with caution on large files.
    fileClass->setMethod("readAll", Value::makeNativeFunction("readAll", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "readAll() called on a closed File", 0, "");
                return Value();
            }
            std::stringstream ss;
            ss << fs->rdbuf();
            return Value(ss.str());
        }));

    // File.write(data) -> true
    fileClass->setMethod("write", Value::makeNativeFunction("write", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "write() called on a closed File", 0, "");
                return Value();
            }
            if (!args[1].isString()) {
                interp.throwException("TypeError", "File.write() expects string data", 0, "");
                return Value();
            }
            *fs << args[1].asString();
            return Value(true);
        }));

    // File.writeLine(data) -> true
    fileClass->setMethod("writeLine", Value::makeNativeFunction("writeLine", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "writeLine() called on a closed File", 0, "");
                return Value();
            }
            if (!args[1].isString()) {
                interp.throwException("TypeError", "File.writeLine() expects string data", 0, "");
                return Value();
            }
            *fs << args[1].asString() << "\n";
            return Value(true);
        }));

    // File.seek(offset) -> true
    fileClass->setMethod("seek", Value::makeNativeFunction("seek", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "seek() called on a closed File", 0, "");
                return Value();
            }
            if (!args[1].isNumber()) {
                interp.throwException("TypeError", "File.seek() expects integer offset", 0, "");
                return Value();
            }
            auto offset = static_cast<std::streamoff>(args[1].asNumber());
            fs->seekg(offset, std::ios::beg);
            fs->seekp(offset, std::ios::beg);
            return Value(true);
        }));

    // File.tell() -> integer
    fileClass->setMethod("tell", Value::makeNativeFunction("tell", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "tell() called on a closed File", 0, "");
                return Value();
            }
            return Value(static_cast<long long>(fs->tellg()));
        }));

    // File.isOpen() -> bool
    fileClass->setMethod("isOpen", Value::makeNativeFunction("isOpen", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            return Value(fs != nullptr && fs->is_open());
        }));

    // File.close()
    fileClass->setMethod("close", Value::makeNativeFunction("close", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (fs && fs->is_open()) {
                fs->close();
            }
            removeStream(instance.get());
            instance->setProperty("_open", Value(false));
            return Value(true);
        }));

    // File.eof() -> bool
    fileClass->setMethod("eof", Value::makeNativeFunction("eof", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                return Value(true); // Closed file is at EOF
            }
            return Value(fs->eof());
        }));

    interp.defineGlobal("File", Value(fileClass));
}
