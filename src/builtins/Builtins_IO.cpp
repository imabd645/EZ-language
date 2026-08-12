#include "runtime/objects/EZObjects.h"
#include "builtins/Builtins.h"
#include "runtime/RuntimeContext.h"
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
// which closes the handle -- so an unreferenced File now closes itself.
static std::mutex g_fileMtx;

struct FileStreamEntry {
    std::shared_ptr<std::fstream> stream;
    std::weak_ptr<EZInstance>     owner;   // expires when the File is collected
};
static std::unordered_map<EZInstance*, FileStreamEntry> g_fileStreams;

// Drop entries whose File instance no longer exists. Called on open, so the
// table is bounded by the number of files actually still reachable.
static void reapDeadStreams_locked() {
    for (auto it = g_fileStreams.begin(); it != g_fileStreams.end(); ) {
        if (it->second.owner.expired()) it = g_fileStreams.erase(it);
        else                            ++it;
    }
}

static void storeStream(const std::shared_ptr<EZInstance>& inst,
                        std::shared_ptr<std::fstream> fs) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    // Reap first: this also removes any stale entry sitting on a recycled
    // address, so the insert below cannot inherit a dead file's stream.
    reapDeadStreams_locked();
    g_fileStreams[inst.get()] = FileStreamEntry{ std::move(fs), inst };
}

static std::shared_ptr<std::fstream> getStream(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    auto it = g_fileStreams.find(inst);
    if (it == g_fileStreams.end()) return nullptr;
    // A live caller holds the instance, so an expired owner here means the entry
    // is stale from a recycled address rather than this file's.
    if (it->second.owner.expired()) {
        g_fileStreams.erase(it);
        return nullptr;
    }
    return it->second.stream;
}

static void removeStream(EZInstance* inst) {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    g_fileStreams.erase(inst);
}

// Release the handles of every File that has been collected.
//
// Reaping on open alone is not enough: a program that drops its last File and
// never opens another would hold that OS handle until it did. gc_collect()
// calls this so there is an explicit, predictable way to release them --
// "collect frees resources" being the reasonable expectation.
void ezReapDeadFileStreams() {
    std::lock_guard<std::mutex> lk(g_fileMtx);
    reapDeadStreams_locked();
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
            // std::ios::binary: without it Windows opens in TEXT mode and
            // collapses every CRLF to LF on the way in, which silently corrupts
            // every non-text file. The matching writeFile() expanded LF back to
            // CRLF, so a read/write round-trip of a PNG grew by one byte per
            // 0x0A in the image and produced a file no decoder would accept.
            // Whole-file I/O must be byte-exact; the line-oriented calls below
            // (readLines/writeLine/appendLine) stay in text mode, where newline
            // translation is exactly what is wanted.
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
            
            // Binary, to match readFile() -- see the note there. In text mode
            // every LF in `content` was written as CRLF, so saving an uploaded
            // image corrupted it.
            std::ofstream file(path, std::ios::binary);
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
            
            std::ofstream file(path, std::ios::app | std::ios::binary);
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
            // Pass the shared_ptr, not the raw pointer: the table keeps a weak
            // reference so it can tell when this File has been collected.
            storeStream(instance, fs);
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

    // File.flush() -> true
    // Pushes buffered writes to the OS. Without this a long-lived writer (a log
    // file, say) keeps its most recent lines in the C++ stream buffer, so they
    // are missing from the file until it is closed -- and lost entirely if the
    // process dies.
    fileClass->setMethod("flush", Value::makeNativeFunction("flush", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "flush() called on a closed File", 0, "");
                return Value();
            }
            fs->flush();
            return Value(true);
        }));

    // File.size() -> integer
    // Size in bytes of the open file. The read/write positions are restored, so
    // this is safe to call in the middle of writing (log rotation checks it on
    // every line).
    fileClass->setMethod("size", Value::makeNativeFunction("size", 0,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            auto instance = args[0].asInstance();
            auto fs = getStream(instance.get());
            if (!fs || !fs->is_open()) {
                interp.throwException("ValueError", "size() called on a closed File", 0, "");
                return Value();
            }
            // Appending streams keep the put pointer at the end but the get
            // pointer at 0, so measure on disk rather than trusting either.
            fs->flush();
            std::error_code ec;
            auto sz = std::filesystem::file_size(
                std::filesystem::u8path(instance->getProperty("_path").asString()), ec);
            if (ec) {
                interp.throwException("IOError",
                    "Could not determine size of '" + instance->getProperty("_path").asString() +
                    "': " + ec.message(), 0, "");
                return Value();
            }
            return Value(static_cast<long long>(sz));
        }));

    // ── Path-level operations ──────────────────────────────────────────────
    // Static, because they act on a path rather than an open handle. Log
    // rotation needs all three: check the file exists, rename it aside, drop
    // the oldest generation.

    // File.exists(path) -> bool
    fileClass->setStaticMember("exists", Value::makeNativeFunction("exists", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.throwException("TypeError", "File.exists() expects a string path", 0, "");
                return Value();
            }
            std::error_code ec;
            return Value(std::filesystem::exists(std::filesystem::u8path(args[0].asString()), ec));
        }));

    // File.remove(path) -> bool   (alias: File.delete)
    // Returns false when the path was already absent, so cleanup code does not
    // have to guard the call. A permission failure still throws.
    auto removeFn = Value::makeNativeFunction("remove", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.throwException("TypeError", "File.remove() expects a string path", 0, "");
                return Value();
            }
            const std::string path = args[0].asString();
            std::error_code ec;
            bool removed = std::filesystem::remove(std::filesystem::u8path(path), ec);
            if (ec) {
                interp.throwException("IOError",
                    "Could not delete '" + path + "': " + ec.message(), 0, "");
                return Value();
            }
            return Value(removed);
        });
    fileClass->setStaticMember("remove", removeFn);
    fileClass->setStaticMember("delete", removeFn);

    // File.rename(from, to) -> true
    // Replaces `to` if it already exists, which is what rotation wants.
    fileClass->setStaticMember("rename", Value::makeNativeFunction("rename", 2,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.size() < 2 || !args[0].isString() || !args[1].isString()) {
                interp.throwException("TypeError", "File.rename() expects two string paths", 0, "");
                return Value();
            }
            const std::string from = args[0].asString();
            const std::string to   = args[1].asString();
            std::error_code ec;
            std::filesystem::rename(std::filesystem::u8path(from),
                                    std::filesystem::u8path(to), ec);
            if (ec) {
                interp.throwException("IOError",
                    "Could not rename '" + from + "' to '" + to + "': " + ec.message(), 0, "");
                return Value();
            }
            return Value(true);
        }));

    // File.size(path) -> integer
    fileClass->setStaticMember("size", Value::makeNativeFunction("size", 1,
        [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
            if (args.empty() || !args[0].isString()) {
                interp.throwException("TypeError", "File.size() expects a string path", 0, "");
                return Value();
            }
            const std::string path = args[0].asString();
            std::error_code ec;
            auto sz = std::filesystem::file_size(std::filesystem::u8path(path), ec);
            if (ec) {
                interp.throwException("IOError",
                    "Could not determine size of '" + path + "': " + ec.message(), 0, "");
                return Value();
            }
            return Value(static_cast<long long>(sz));
        }));

    interp.defineGlobal("File", Value(fileClass));
}
