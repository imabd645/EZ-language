#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include <algorithm>
#include <functional>
#include "vm/BytecodeVM.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include "lexer/Lexer.h"
#include "parser/Parser.h"
#include "ast/ASTArena.h"
#include "builtins/Builtins.h"
#include "gui/GUIBuiltins.h"
#include "eventloop/EventLoop.h"
#include "runtime/EZFuture.h"
#include <thread>
#include <sqlite3.h>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define NOCOMM
#ifdef _WIN32
#include <windows.h>
#endif
// The Windows COM headers define INTERFACE as a macro used for vtable
// declarations. If it is still defined here it will corrupt any subsequent
// reference to ValueType::INTERFACE, so we remove it unconditionally.
#ifdef INTERFACE
#  undef INTERFACE
#endif

// ============================================================================
// Global Source Registry Ã¢â‚¬â€ maps filename -> vector of source lines
// Populated by the Lexer/interpreter when loading each file so that
// runtimeError() can print the offending source line.
// ============================================================================
static std::unordered_map<std::string, std::vector<std::string>> g_sourceRegistry;

void EZ_RegisterSource(const std::string& filename, const std::string& source) {
    std::vector<std::string> lines;
    std::istringstream ss(source);
    std::string ln;
    while (std::getline(ss, ln)) lines.push_back(ln);
    g_sourceRegistry[filename] = std::move(lines);
}

const std::string* EZ_GetSourceLine(const std::string& filename, int line) {
    auto it = g_sourceRegistry.find(filename);
    if (it == g_sourceRegistry.end()) return nullptr;
    int idx = line - 1;
    if (idx < 0 || idx >= (int)it->second.size()) return nullptr;
    return &it->second[idx];
}

// ============================================================================
// BytecodeVM Implementation
// ============================================================================

BytecodeVM::BytecodeVM(size_t stackSize)
    : stackMax(stackSize), stackTop(nullptr), openUpvalues(nullptr),
      isExceptionPending(false), running(false) {
    stack.resize(stackMax);
    stackTop = stack.data();
    frames.reserve(1024);
    frameUpvalues.reserve(1024);
    globalEnv = std::make_shared<Environment>();
        initBuiltins();
}

BytecodeVM::BytecodeVM(std::shared_ptr<Environment> globalEnv_, size_t stackSize)
    : globalEnv(globalEnv_), stackMax(stackSize), stackTop(nullptr), openUpvalues(nullptr),
      isExceptionPending(false), running(false) {
    stack.resize(stackMax);
    stackTop = stack.data();
    frames.reserve(1024);
    frameUpvalues.reserve(1024);
        initBuiltins();
}

BytecodeVM::~BytecodeVM() {
    // Close any still-open upvalues before our stack is destroyed, so a closure
    // that escaped this VM (e.g. returned from a spawn()/async worker) keeps a
    // self-contained value in `closed` instead of a dangling pointer into freed
    // stack memory. Closed upvalues are still co-owned by those closures.
    closeUpvalues(stack.data());
}

Environment::~Environment() {
    for (auto& pair : persistDBConnections) {
        if (pair.second) {
            sqlite3_close(static_cast<sqlite3*>(pair.second));
        }
    }
}

void BytecodeVM::initGlobalSlots(const std::vector<std::string>& slotNames) {
    std::unique_lock<std::shared_mutex> lock(globalEnv->slotMutex);
    globalEnv->globalSlotNames = slotNames;
    globalEnv->globalSlots.resize(slotNames.size(), Value());
    // Pre-seed slots from globalEnv for any built-in names that share a slot name
    for (size_t i = 0; i < slotNames.size(); ++i) {
        if (!slotNames[i].empty() && globalEnv->contains(slotNames[i])) {
            globalEnv->globalSlots[i] = globalEnv->get(slotNames[i]);
        }
    }
}

Value BytecodeVM::execute(BytecodeFunctionPtr function) {
    return execute(function, {});
}



Value BytecodeVM::execute(BytecodeFunctionPtr function,
                           const std::vector<Value>& args) {
    // Ensure constants are resolved for this function and all nested ones
    function->chunk.resolveConstants();
    
    // Recursive resolution helper
    std::function<void(BytecodeFunctionPtr)> resolveRecursive = [&](BytecodeFunctionPtr f) {
        f->chunk.resolveConstants();
        for (auto& nested : f->nestedFunctions) {
            resolveRecursive(nested);
        }
    };
    resolveRecursive(function);

    // Save current state to support re-entrant calls (e.g. from builtins or constructors)
    auto savedFrames = std::move(frames);
    auto savedFrameUpvalues = std::move(frameUpvalues);
    auto savedTryStack = std::move(tryStack);
    size_t stackOffset = stackTop - stack.data();
    bool savedRunning = running;
    bool savedException = isExceptionPending;

    // Reset execution state for THIS recursive run
    frames.clear();
    frameUpvalues.clear();
    tryStack.clear();
    // Do NOT reset stackTop to stack.data()! We append to the existing stack to preserve outer frames.
    running = true;
    isExceptionPending = false;
    // Push a dummy callee (nil) so that slots[-1] is valid for the main frame
    push(Value());

    // Push arguments
    for (const auto& arg : args) push(arg);

    // Push the main call frame
    CallFrame frame;
    frame.function     = function;
    frame.ip           = function->chunk.code.data();
    frame.slots        = stackTop - args.size(); // arg0 is at slots[0]
    
    // Advance stackTop to account for main function locals
    while ((stackTop - frame.slots) < (long long)function->localCount) {
        push(Value());
    }

    frame.functionName = function->name;
    frame.filename     = function->filename;
    frame.line         = 0;
    frame.localCount   = function->localCount;
    frames.push_back(frame);
    frameUpvalues.push_back(ClosureState{});

    // Putting the outer state back is NOT optional on the exception path.
    //
    // This run has an empty tryStack, so a throw the callee does not handle
    // reaches runtimeError(), which throws a C++ RuntimeError, and run() rethrows
    // anything whose handler it does not own. Either way the exception unwinds
    // through here. With the restore written inline after run() it was skipped,
    // and savedFrames -- a local -- was destroyed on the way out, taking the
    // caller's frames with it.
    //
    // (This is the top-level entry point. The re-entrant path that constructors
    // and FFI callbacks take is callFunction(), which had the same hole plus a
    // tryStack-ownership bug of its own.)
    auto restoreState = [&]() {
        frames = std::move(savedFrames);
        frameUpvalues = std::move(savedFrameUpvalues);
        tryStack = std::move(savedTryStack);
        stackTop = stack.data() + stackOffset;
        running = savedRunning;
        isExceptionPending = savedException;
    };

    try {
        run(frames.size());
    } catch (...) {
        restoreState();
        throw;   // let the caller's handler see it, with the VM intact
    }

    // A yielded coroutine has NOT finished: its frames must stay live for the
    // resume, so this path deliberately leaves the state alone.
    if (isYielded) return Value();

    // An uncaught top-level exception leaves run() via pendingException + return
    // rather than unwinding a C++ exception out of the dispatch loop. Turn that
    // back into a RuntimeError here so the process reports failure.
    //
    // Returning normally used to mean a script killed by an uncaught error still
    // exited 0. Nothing downstream could tell a crash from success: the test
    // runner passed anything that died, and test_builtin.ez -- which threw on
    // its first line and never ran another statement -- sat green for as long as
    // it existed. verify_static.ez did the same while its feature was outright
    // broken.
    //
    // The error text was already printed by runtimeError(), so callers that just
    // want the exit status (CLI -> exit 70) need print nothing more. The REPL
    // catches std::exception and keeps the session alive, and a module that dies
    // while loading now fails the load instead of yielding a half-built module.
    if (isExceptionPending || !pendingException.isNil()) {
        std::string detail = pendingException.isNil() ? std::string("uncaught error")
                                                      : pendingException.toString();
        restoreState();
        throw RuntimeError("uncaught: " + detail);
    }

    // Read the result before restoring, while stackTop still points at it.
    Value result = (stackTop > stack.data()) ? *(stackTop - 1) : Value();

    restoreState();

    return result;
}

// ============================================================================
// Main Execution Loop
// ============================================================================

void BytecodeVM::runtimeError(const std::string& message, int line, const std::string& filename) {
    if (pendingException.isNil()) {
        pendingException = Value(message);
    }

    // Resolve the actual fault location: prefer the caller-supplied line/file,
    // fall back to the top CallFrame's current line and filename.
    int    faultLine = line;
    std::string faultFile = filename;
    if (faultLine <= 0 && !frames.empty()) {
        faultLine = frames.back().line;
    }
    if (faultFile.empty() && !frames.empty()) {
        faultFile = frames.back().filename;
    }

    if (tryStack.empty() && !isAsyncTask) {
        // ── Print the error header ────────────────────────────────────────────────────────────
#ifdef _WIN32
        // Use ANSI escapes if the terminal supports them (Windows 10+)
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD  consoleMode = 0;
        bool   ansi = GetConsoleMode(hErr, &consoleMode) != 0;
        if (ansi) SetConsoleMode(hErr, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
        bool ansi = true; // Assume POSIX terminals support ANSI
#endif

        auto RED    = ansi ? "\033[1;31m" : "";
        auto YELLOW = ansi ? "\033[1;33m" : "";
        auto CYAN   = ansi ? "\033[0;36m" : "";
        auto BOLD   = ansi ? "\033[1m"    : "";
        auto RESET  = ansi ? "\033[0m"    : "";

        std::string errorType = "Error";
        if (pendingException.isInstance()) {
            errorType = pendingException.asInstance()->klass->name;
        }

        std::cerr << "\n" << RED << errorType << RESET << ": " << BOLD << message << RESET << "\n";

        // Ã¢â€â‚¬Ã¢â€â‚¬ Location line Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        if (!faultFile.empty() || faultLine > 0) {
            std::cerr << "  " << CYAN;
            if (!faultFile.empty()) std::cerr << faultFile;
            if (!faultFile.empty() && faultLine > 0) std::cerr << ":";
            if (faultLine > 0) std::cerr << faultLine;
            std::cerr << RESET << "\n";
        }

        // Ã¢â€â‚¬Ã¢â€â‚¬ Source snippet Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        if (faultLine > 0) {
            const std::string* srcLine = EZ_GetSourceLine(faultFile, faultLine);
            if (srcLine) {
                // Trim leading whitespace for display, but keep a counter for caret
                size_t indent = srcLine->find_first_not_of(" \t");
                if (indent == std::string::npos) indent = 0;
                std::cerr << "  " << YELLOW << std::to_string(faultLine) << " |" << RESET << "  "
                          << srcLine->substr(indent) << "\n";
                std::cerr << "      " << RED << "^^^" << RESET << "\n";
            }
        }

        // Ã¢â€â‚¬Ã¢â€â‚¬ Stack trace Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        printStackTrace();
    }

    running = false;

    // faultMode is set while a doXXX helper runs (see guardedHelper). In that
    // window we must NOT throw a C++ exception: the helper is deep inside run()'s
    // computed-goto dispatch, and when that dispatch is itself running inside the
    // libuv event-loop callback (every FFI callback / ezweb handler), a C++
    // exception cannot unwind at all -- verified with gdb, it skips every catch
    // on the stack (helper, run, callFunction, the uv callback) and jumps to a
    // null handler, crashing the process. Instead the fault travels as
    // running=false + pendingException, and the dispatch routes it to
    // handle_vm_fault with a plain goto. This is how a handler that indexes nil
    // now produces a catchable EZ error instead of taking the server down.
    if (faultMode) {
        return;
    }
    throw RuntimeError(message, faultLine);
}

void BytecodeVM::throwException(const std::string& className, const std::string& message, int line, const std::string& filename) {
    Value classVal = globalEnv->get(className);
    if (classVal.isClass()) {
        auto inst = std::make_shared<EZInstance>(classVal.asClass());
        CycleCollector::instance().track(inst, ValueType::INSTANCE);
        inst->setProperty("message", Value(message));
        
        int faultLine = line > 0 ? line : (frames.empty() ? 0 : frames.back().line);
        if (pendingException.isNil()) {
            pendingException = Value(inst);
        }
        
        // Let runtimeError handle printing if uncaught, passing the instance inside the C++ exception
        // Note: We bypass runtimeError here so it doesn't print immediately, but instead we just throw!
        // Wait, if it's uncaught, we want runtimeError's exact formatting logic. 
        // Actually, if we just throw, the catch block at top level will leave pendingException set 
        // and the VM will print it? 
        // Let's print using runtimeError logic if tryStack is empty!
        
        if (tryStack.empty() && !isAsyncTask) {
            runtimeError(message, faultLine, filename); // This will print and throw a standard RuntimeError (which is fine, script dies)
        } else {
            throw RuntimeError(message, faultLine, Value(inst));
        }
        return;
    }
    // Fallback if the class isn't defined
    runtimeError(message, line, filename);
}


void BytecodeVM::initBuiltins() {
    registerBuiltins(*this);
    registerGUIBuiltins(*this);

    // Ã¢â€â‚¬ Decorator builtins Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬

    // audit(obj) Ã¢â€ â€™ list of dicts with field/old/new/via/timestamp
    defineGlobal("audit", Value::makeNativeFunction("audit", 1, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isInstance()) { ctx.runtimeError("audit() expects a model instance"); return Value(); }
        auto inst = args[0].asInstance();
        if (!inst->klass->behaviors.audited) { ctx.runtimeError("audit() called on non-@audited model '" + inst->klass->name + "'"); return Value(); }
        auto result = std::make_shared<EZArray>();
        if (inst->getAuditLog()) {
            for (const auto& e : *inst->getAuditLog()) {
                auto d = std::make_shared<EZDictionary>();
                d->modifyMap([&](auto& m) { m["field"]     = Value(e.field); });
                d->modifyMap([&](auto& m) { m["old"]       = e.oldValue; });
                d->modifyMap([&](auto& m) { m["new"]       = e.newValue; });
                d->modifyMap([&](auto& m) { m["via"]       = Value(e.via); });
                d->modifyMap([&](auto& m) { m["timestamp"] = Value(e.timestamp); });
                result->modifyElements([&](auto& el) { el.push_back(Value(d)); });
            }
        }
        return Value(result);
    }));

    // audit_clear(obj) Ã¢â€ â€™ clears audit log
    defineGlobal("audit_clear", Value::makeNativeFunction("audit_clear", 1, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (args[0].isInstance() && args[0].asInstance()->getAuditLog())
            args[0].asInstance()->modifyAuditLog([](auto& log) { log.clear(); });
        return Value();
    }));

    // audit_since(obj, timestamp) Ã¢â€ â€™ list of entries since timestamp
    defineGlobal("audit_since", Value::makeNativeFunction("audit_since", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isInstance()) { ctx.runtimeError("audit_since() expects a model instance"); return Value(); }
        auto inst = args[0].asInstance();
        long long since = args[1].isInteger() ? args[1].asInteger() : (long long)args[1].asFloat();
        auto result = std::make_shared<EZArray>();
        if (inst->getAuditLog()) {
            for (const auto& e : *inst->getAuditLog()) {
                if (e.timestamp < since) continue;
                auto d = std::make_shared<EZDictionary>();
                d->modifyMap([&](auto& m) { m["field"]     = Value(e.field); });
                d->modifyMap([&](auto& m) { m["old"]       = e.oldValue; });
                d->modifyMap([&](auto& m) { m["new"]       = e.newValue; });
                d->modifyMap([&](auto& m) { m["via"]       = Value(e.via); });
                d->modifyMap([&](auto& m) { m["timestamp"] = Value(e.timestamp); });
                result->modifyElements([&](auto& el) { el.push_back(Value(d)); });
            }
        }
        return Value(result);
    }));

    // snapshot(obj) Ã¢â€ â€™ dict copy of current properties
    defineGlobal("snapshot", Value::makeNativeFunction("snapshot", 1, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isInstance()) { ctx.runtimeError("snapshot() expects a model instance"); return Value(); }
        auto inst = args[0].asInstance();
        if (!inst->klass->behaviors.snapshot) { ctx.runtimeError("snapshot() called on non-@snapshot model '" + inst->klass->name + "'"); return Value(); }
        auto snap = std::make_shared<EZDictionary>();
        {
            for (const auto& [k, v] : inst->getPropertiesCopy()) snap->modifyMap([&](auto& m) { m[k] = v; });
        }
        return Value(snap);
    }));

    // rollback(obj, snap) Ã¢â€ â€™ restore properties from snapshot dict
    defineGlobal("rollback", Value::makeNativeFunction("rollback", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isInstance()) { ctx.runtimeError("rollback() expects a model instance"); return Value(); }
        auto inst = args[0].asInstance();
        if (!inst->klass->behaviors.snapshot) { ctx.runtimeError("rollback() called on non-@snapshot model '" + inst->klass->name + "'"); return Value(); }
        if (!args[1].isDictionary()) { ctx.runtimeError("rollback() second argument must be a snapshot dict"); return Value(); }
        auto snap = args[1].asDictionaryPtr();
        for (const auto& [k, v] : snap->getMapCopy()) inst->setProperty(k, v);
        return Value();
    }));

    // snapshot_diff(a, b) Ã¢â€ â€™ dict of changed fields {was, now}
    defineGlobal("snapshot_diff", Value::makeNativeFunction("snapshot_diff", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isDictionary() || !args[1].isDictionary()) {
            ctx.runtimeError("snapshot_diff() expects two snapshot dicts"); return Value();
        }
        auto a = args[0].asDictionaryPtr();
        auto b = args[1].asDictionaryPtr();
        auto result = std::make_shared<EZDictionary>();
        // Fix 2.4: take both map copies once — previous code called getMapCopy() O(n) times inside the loop
        auto aMap = a->getMapCopy();
        auto bMap = b->getMapCopy();
        for (const auto& [k, vb] : bMap) {
            auto it = aMap.find(k);
            Value va = (it != aMap.end()) ? it->second : Value();
            if (va.toString() != vb.toString()) {
                auto entry = std::make_shared<EZDictionary>();
                entry->modifyMap([&](auto& m) { m["was"] = va; });
                entry->modifyMap([&](auto& m) { m["now"] = vb; });
                result->modifyMap([&](auto& m) { m[k] = Value(entry); });
            }
        }
        return Value(result);
    }));
}

// ============================================================================
// Debugging
// ============================================================================

void BytecodeVM::printStack() const {
    std::cout << "          ";
    for (const Value* slot = stack.data(); slot < stackTop; slot++) {
        std::cout << "[" << slot->toString() << "]";
    }
    std::cout << std::endl;
}

// ============================================================================
// Compile EZFunction to Bytecode (on-demand)
// ============================================================================

BytecodeFunctionPtr BytecodeVM::compileEZFunction(EZFunction* func) {
    {
        std::shared_lock<std::shared_mutex> lock(globalEnv->registryMutex);
        auto it = globalEnv->compiledFunctionCache.find(func);
        if (it != globalEnv->compiledFunctionCache.end()) return it->second;
    }

    ASTArena arena;
    BytecodeCompiler compiler(arena);

    // Build a minimal TaskStmt from the EZFunction
    TaskStmt fakeTask(func->name, func->params, std::vector<TypeASTPtr>(func->params.size(), arena.allocate<TypeAST>("Any")), func->defaultValues, nullptr,
                      func->body, func->isVariadic);
    BytecodeFunctionPtr bfunc = nullptr;
    try {
        bfunc = compiler.compileFunction(fakeTask, func->name);
    } catch (const CompilerError& e) {
        // Since this is a lazy evaluation of an async lambda, we must fail gracefully
        runtimeError("JIT Compilation Error: " + std::string(e.what()));
        return nullptr;
    }

    {
        std::unique_lock<std::shared_mutex> lock(globalEnv->registryMutex);
        globalEnv->compiledFunctionCache[func] = bfunc;
    }
    
    // Clear AST data to free memory, as it's no longer needed after bytecode compilation.
    // The AST statements hold large tree structures that are expensive to keep around.
    // Since EZ-language now executes strictly via bytecode, the runtime only needs the BytecodeFunctionPtr.
    func->body.clear();
    func->defaultValues.clear();
    
    return bfunc;
}

// ============================================================================
// callFunction (from native / external code)
// ============================================================================

// ============================================================================
// Attribute hooks: __getattr__ / __setattr__
// ============================================================================

// A hook body will normally touch the very object it was called for. There is
// no re-entrancy guard, deliberately -- the same contract as Lua's __index and
// __newindex:
//
//   * inside __setattr__, write with setattr(self, name, value)
//   * inside __getattr__, read with getattr(self, name)
//
// Those builtins go straight to EZInstance::setProperty/getProperty rather than
// through these opcodes, so they do not re-enter the hook. Writing `self.x = v`
// inside __setattr__ recurses, which is a programming error and surfaces as an
// ordinary call-depth error rather than a crash.
//
// The dunder names themselves are excluded so that looking up "__getattr__" on a
// class that has none is a plain miss rather than a self-call.

Value BytecodeVM::findGetattrHook(const Value& obj, const std::string& name) {
    if (name == "__getattr__" || name == "__setattr__") return Value();

    if (obj.isInstance()) {
        auto inst = obj.asInstance();
        if (!inst->klass || !inst->klass->hasGetattrHook) return Value();
        return inst->klass->findMethod("__getattr__");
    }
    if (obj.isClass()) {
        // A class-level miss looks for a STATIC __getattr__(cls, name), so that
        // Model.column can resolve without an instance -- what an ORM needs in
        // order to hand out column handles for query building.
        auto klass = obj.asClass();
        if (!klass->hasGetattrHook) return Value();
        return klass->findStaticMember("__getattr__");
    }
    return Value();
}

Value BytecodeVM::findSetattrHook(const Value& obj, const std::string& name) {
    // Ordered so the common case -- an ordinary class with no hook -- costs one
    // predictable branch on a bool and nothing else. This runs on every property
    // write, so the string comparisons below must not be reached by default.
    if (!obj.isInstance()) return Value();
    auto inst = obj.asInstance();
    if (!inst->klass || !inst->klass->hasSetattrHook) return Value();
    if (name == "__getattr__" || name == "__setattr__") return Value();
    return inst->klass->findMethod("__setattr__");
}

Value BytecodeVM::callFunction(const Value& callee,
                                const std::vector<Value>& args,
                                int line,
                                const std::string& filename) {
    if (callee.isNativeFunction()) {
        try {
            return callee.asNativeFunction()->function(*this, args);
        } catch (const std::exception& e) {
            runtimeError(std::string("Native function error: ") + e.what(), line, filename);
            return Value();
        }
    }

    // Save current stack state to prevent corruption if dispatchCall fails
    size_t stackBefore = stackTop - stack.data();
    size_t framesBefore = frames.size();
    bool savedRunning = running;
    running = true;

    push(callee);
    for (const auto& arg : args) {
        push(arg);
    }

    // Restoring the stack/frames is not optional on the exception path.
    //
    // The nested run() below now propagates a C++ RuntimeError when the callee
    // throws and the only handler lives in one of OUR caller's frames (see
    // ownsTryBlock() in run()). Before that, the callee's throw unwound straight
    // into the caller's handler from INSIDE the nested run: the caller's
    // bytecode resumed there, and then the rewind below ran anyway and pulled
    // the stack out from under it. The outer run() then carried on with a stale
    // value sitting where a callee should be:
    //
    //     model Boom { init() { throw "b" } }
    //     try { Boom() } catch (e) { }   # prints fine
    //     Fine(3)                        # -> "Value is not callable: string"
    //
    // Constructors arrive here via instantiate(); FFI callbacks, sort
    // comparators and any other builtin that calls back into EZ do too.
    auto restoreState = [&]() {
        stackTop = stack.data() + stackBefore;
        frames.resize(framesBefore);
        running = savedRunning;
    };

    Value result;
    bool propagate = false;
    Value propagated;
    try {
        if (dispatchCall(callee, args.size())) {
            run(frames.size());

            // A yielded coroutine has NOT finished: its frames must stay live
            // for the resume, so this path deliberately skips the restore.
            if (isYielded) return Value();

            // run() hands an exception it does not own back through
            // pendingException + return, rather than unwinding a C++ exception
            // out of its computed-goto dispatch (which corrupts the unwinder,
            // fatally so under the event loop). Pick it up here and re-raise it
            // from THIS ordinary function, which unwinds safely into the caller's
            // dispatch -- the run() that owns the handler catches it there.
            if (!pendingException.isNil()) {
                propagate = true;
                propagated = pendingException;
                pendingException = Value();
            } else if (stackTop > stack.data() + stackBefore) {
                result = *(stackTop - 1);
            }
        }
    } catch (...) {
        // A genuinely re-entrant builtin (not the VM's own dispatch) can still
        // throw a C++ exception directly; keep it moving.
        restoreState();
        throw;
    }

    // Always restore stack to exactly where it was before the call
    restoreState();

    if (propagate) {
        throw RuntimeError(
            propagated.isDictionary() && propagated.asDictionaryPtr()->has("message")
                ? propagated.asDictionaryPtr()->get("message").toString()
                : propagated.toString(),
            0, propagated);
    }

    return result;
}

Value BytecodeVM::instantiate(std::shared_ptr<EZClass> klass,
                               const std::vector<Value>& args,
                               int line,
                               const std::string& filename) {
    auto inst = std::make_shared<EZInstance>(klass);
    CycleCollector::instance().track(inst, ValueType::INSTANCE);
    Value instVal(inst);

    Value init = Value();
    auto currentClass = klass;
    while (currentClass) {
        if (currentClass->methods.count("init")) {
            init = currentClass->methods.at("init");
            break;
        }
        currentClass = currentClass->parent;
    }

    if (!init.isNil()) {
        std::vector<Value> initArgs = { instVal };
        initArgs.insert(initArgs.end(), args.begin(), args.end());
        
        // Execute constructor through callFunction (handles bytecode or native)
        callFunction(init, initArgs, line, filename);
    }

    return instVal;
}

void BytecodeVM::printStackTrace() const {
    // Collect frames innermost-first, skip duplicate <main> frames
    struct TraceEntry { 
        std::string func; 
        std::string file; 
        int line; 
        std::vector<std::pair<std::string, std::string>> locals;
    };
    std::vector<TraceEntry> trace;
    for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
        int currentLine = it->line;
        size_t pc = 0;
        if (it->function && !it->function->chunk.lines.empty()) {
            size_t offset = (size_t)(it->ip - it->function->chunk.code.data());
            if (offset > 0) offset--;
            pc = offset;
            if (offset < it->function->chunk.lines.size()) {
                currentLine = (int)it->function->chunk.lines[offset];
            }
        }
        
        std::vector<std::pair<std::string, std::string>> activeLocals;
        if (it->function) {
            for (const auto& var : it->function->localVars) {
                if (var.startPC <= pc && pc <= var.endPC) {
                    // Make sure the slot is within the currently accessible stack frame
                    if (var.slot < (size_t)(stackTop - it->slots)) {
                        activeLocals.push_back({var.name, (it->slots + var.slot)->toString()});
                    }
                }
            }
        }
        
        std::string fn = it->functionName.empty() ? "<script>" : it->functionName;
        trace.push_back({fn, it->filename, currentLine, activeLocals});
    }

    if (trace.empty()) return;
    std::cerr << "\nTraceback (most recent call last):\n";
    // Print outermost first (reverse of our innermost-first vector)
    for (auto tit = trace.rbegin(); tit != trace.rend(); ++tit) {
        std::cerr << "  File \"" << (tit->file.empty() ? "<unknown>" : tit->file)
                  << "\", line " << (tit->line > 0 ? tit->line : 0)
                  << ", in " << tit->func << "\n";
        // Show source snippet if available
        if (tit->line > 0) {
            const std::string* srcLine = EZ_GetSourceLine(tit->file, tit->line);
            if (srcLine) {
                size_t indent = srcLine->find_first_not_of(" \t");
                if (indent == std::string::npos) indent = 0;
                std::cerr << "    " << srcLine->substr(indent) << "\n";
            }
        }
        // Dump local variables
        if (!tit->locals.empty()) {
            std::cerr << "    Local variables:\n";
            for (const auto& local : tit->locals) {
                std::cerr << "      " << local.first << " = " << local.second << "\n";
            }
        }
    }
}
