#include <algorithm>
#include <functional>
#include "BytecodeVM.h"
#include <iostream>
#include <cmath>
#include <sstream>
#include "Lexer.h"
#include "Parser.h"
#include "Builtins.h"
#include "GUIBuiltins.h"
#include "runtime/EventLoop.h"
#include "EZFuture.h"
#include <thread>
#include <sqlite3.h>

#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#define NOCOMM
#include <windows.h>
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

BytecodeVM::BytecodeVM()
    : stackTop(nullptr), openUpvalues(nullptr),
      isExceptionPending(false), running(false) {
    stack.resize(STACK_MAX);
    stackTop = stack.data();
    frames.reserve(1024);
    frameUpvalues.reserve(1024);
    globalEnv = std::make_shared<Environment>();
        initBuiltins();
}

BytecodeVM::BytecodeVM(std::shared_ptr<Environment> globalEnv_)
    : globalEnv(globalEnv_), stackTop(nullptr), openUpvalues(nullptr),
      isExceptionPending(false), running(false) {
    stack.resize(STACK_MAX);
    stackTop = stack.data();
    frames.reserve(1024);
    frameUpvalues.reserve(1024);
        initBuiltins();
}

BytecodeVM::~BytecodeVM() {
    for (auto& pair : persistDBConnections) {
        if (pair.second) {
            sqlite3_close(static_cast<sqlite3*>(pair.second));
        }
    }
    // allUpvalues unique_ptrs handle cleanup automatically
}

void BytecodeVM::initGlobalSlots(const std::vector<std::string>& slotNames) {
    globalSlotNames = slotNames;
    globalSlots.resize(slotNames.size(), Value());
    // Pre-seed slots from globalEnv for any built-in names that share a slot name
    for (size_t i = 0; i < slotNames.size(); ++i) {
        if (!slotNames[i].empty() && globalEnv->contains(slotNames[i])) {
            globalSlots[i] = globalEnv->get(slotNames[i]);
        }
    }
}

Value BytecodeVM::execute(BytecodeFunctionPtr function) {
    return execute(function, {});
}

BytecodeVM::ThreadState BytecodeVM::exportThreadState() const {
    ThreadState state;
    return state;
}

void BytecodeVM::importThreadState(const ThreadState& state) {
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

    run(frames.size());

    if (isYielded) return Value();

    // Result is the top value before we restore
    Value result = (stackTop > stack.data()) ? *(stackTop - 1) : Value();

    // Restore saved state
    frames = std::move(savedFrames);
    frameUpvalues = std::move(savedFrameUpvalues);
    tryStack = std::move(savedTryStack);
    stackTop = stack.data() + stackOffset;
    running = savedRunning;
    isExceptionPending = savedException;

    return result;
}

// ============================================================================
// Main Execution Loop
// ============================================================================

void BytecodeVM::run(size_t targetFrameCount) {
    if (frames.empty()) return;
    size_t startingFrameCount = (targetFrameCount > 0) ? targetFrameCount : frames.size();
    CallFrame* frame = &frames.back();
    const uint8_t* ip = frame->ip;
    Value* stackTop = this->stackTop;
    
#define SYNC_IP() { if (!frames.empty()) frame->ip = ip; this->stackTop = stackTop; }
#define LOAD_FRAME() { frame = &frames.back(); ip = frame->ip; stackTop = this->stackTop; }
#define REFRESH_FRAME() { frame = &frames.back(); }

#define CHECK_VISIBILITY(klass, propName) { \
    auto it = (klass)->visibility.find(propName); \
    if (it != (klass)->visibility.end() && !it->second) { \
        bool allowed = false; \
        if (!frame->function->className.empty()) { \
            auto currentClass = (klass); \
            while (currentClass) { \
                if (currentClass->name == frame->function->className) { \
                    allowed = true; \
                    break; \
                } \
                currentClass = currentClass->parent; \
            } \
        } \
        if (!allowed) { \
            SYNC_IP(); \
            runtimeError("Cannot access hidden member '" + (propName) + "' of model '" + (klass)->name + "'."); \
            return; \
        } \
    } \
}

#define READ_BYTE()   (*ip++)
#define READ_SHORT()  (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))
#define READ_INT()    (ip += 4, (uint32_t)((ip[-4] << 24) | (ip[-3] << 16) | (ip[-2] << 8) | ip[-1]))
#define READ_CONST()  (frame->function->chunk.getConstant(READ_SHORT()))

#ifdef __GNUC__
    static void* dispatchTable[] = {
        &&handle_LOAD_CONST, &&handle_LOAD_LOCAL, &&handle_STORE_LOCAL, &&handle_LOAD_UPVALUE,
        &&handle_STORE_UPVALUE, &&handle_LOAD_GLOBAL, &&handle_STORE_GLOBAL, &&handle_LOAD_PROPERTY,
        &&handle_STORE_PROPERTY, &&handle_POP, &&handle_DUP, &&handle_DUP2,
        &&handle_LOAD_NIL, &&handle_LOAD_TRUE, &&handle_LOAD_FALSE, &&handle_LOAD_ZERO,
        &&handle_LOAD_ONE, &&handle_LOAD_EMPTY_STR, &&handle_INC_LOCAL, &&handle_DEC_LOCAL, &&handle_ADD, &&handle_SUB,
        &&handle_MUL, &&handle_DIV, &&handle_MOD, &&handle_POW,
        &&handle_NEGATE, &&handle_BIT_AND, &&handle_BIT_OR, &&handle_BIT_XOR,
        &&handle_BIT_NOT, &&handle_SHIFT_LEFT, &&handle_SHIFT_RIGHT, &&handle_EQUAL,
        &&handle_NOT_EQUAL, &&handle_LESS, &&handle_LESS_EQ, &&handle_GREATER,
        &&handle_GREATER_EQ, &&handle_NOT, &&handle_JUMP, &&handle_JUMP_IF_FALSE,
        &&handle_JUMP_IF_TRUE, &&handle_JUMP_IF_NIL, &&handle_JUMP_IF_NOT_NIL, &&handle_LOOP, &&handle_CALL, &&handle_TAIL_CALL, &&handle_CALL_KW,
        &&handle_RETURN, &&handle_CLOSURE, &&handle_CLOSE_UPVALUE, &&handle_MAKE_ARRAY,
        &&handle_BUILD_TUPLE, &&handle_MAKE_DICT, &&handle_INDEX_GET, &&handle_INDEX_SET, &&handle_ARRAY_APPEND,
        &&handle_ARRAY_EXTEND, &&handle_CALL_SPREAD, &&handle_NEW_INSTANCE, &&handle_GET_METHOD, &&handle_SUPER, &&handle_SUPER_CALL,
        &&handle_GET_ITER, &&handle_GET_DICT_ITER, &&handle_ITER_NEXT, &&handle_ITER_HAS_NEXT, &&handle_TRY_START,
        &&handle_TRY_END, &&handle_THROW, &&handle_TO_STRING, &&handle_PRINT, &&handle_CLOCK,
        &&handle_TYPE_OF, &&handle_IS_INSTANCE_OF, &&handle_OP_AWAIT, &&handle_MAKE_INTERFACE, &&handle_MAKE_CLASS, &&handle_BREAKPOINT, &&handle_LINE,
        &&handle_HAS_GLOBAL,
        &&handle_LOAD_GLOBAL_SLOT,
        &&handle_STORE_GLOBAL_SLOT,
        &&handle_LOOP_LESS_EQ_LOCAL,
        &&handle_LOOP_GREATER_EQ_LOCAL,
        &&handle_INTERCEPTED_STORE_PROPERTY,
        &&handle_RATELIMIT_CHECK,
        &&handle_GET_CACHED_RESULT,
        &&handle_STORE_CACHED_RESULT,
        &&handle_END
    };
    #define DISPATCH() { \
        if (traceExecution) std::cerr << "[VM-TRACE] OP: " << (int)(*ip) << " at IP: " << (void*)ip << std::endl; \
        goto *dispatchTable[READ_BYTE()]; \
    }
    #define INTERPRET_LOOP DISPATCH();
    #define CASE_CODE(name) handle_##name:
#else
    #define DISPATCH() break
    #define INTERPRET_LOOP while (running && frames.size() >= startingFrameCount) { \
        if (traceExecution) std::cerr << "[VM-TRACE] OP: " << (int)(*ip) << " at IP: " << (void*)ip << std::endl; \
        uint8_t instruction = READ_BYTE(); \
        switch (static_cast<OpCode>(instruction)) \
    #define CASE_CODE(name) case OpCode::name:
#endif

    dispatch_start:
    try {
        INTERPRET_LOOP {
            SYNC_IP();
                CASE_CODE(LOAD_CONST) {
                    uint16_t idx = READ_SHORT();
                    *stackTop++ = frame->function->chunk.resolvedConstants[idx];
                    DISPATCH();
                }

                CASE_CODE(LOAD_LOCAL) {
                    uint8_t slot = READ_BYTE();
                    *stackTop++ = frame->slots[slot];
                    DISPATCH();
                }
                CASE_CODE(STORE_LOCAL) {
                    uint8_t slot = READ_BYTE();
                    frame->slots[slot] = *(stackTop - 1);
                    DISPATCH();
                }

                CASE_CODE(LOAD_UPVALUE) {
                    uint8_t slot = READ_BYTE();
                    const ClosureState& cs = frameUpvalues.back();
                    if (slot < cs.upvalues.size() && cs.upvalues[slot]) {
                        Value* loc = cs.upvalues[slot]->location.load();
                        if (loc) {
                            *stackTop++ = *loc;
                        } else {
                            *stackTop++ = Value();
                        }
                    } else {
                        *stackTop++ = Value();
                    }
                    DISPATCH();
                }
                CASE_CODE(STORE_UPVALUE) {
                    uint8_t slot = READ_BYTE();
                    ClosureState& cs = frameUpvalues.back();
                    if (slot < cs.upvalues.size() && cs.upvalues[slot]) {
                        Value* loc = cs.upvalues[slot]->location.load();
                        if (loc) {
                            *loc = *(stackTop - 1);
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_GLOBAL) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value v = globalEnv->get(name, frame->line);
                        if (v.isNil() && !globalEnv->contains(name)) {
                            SYNC_IP();
                            runtimeError("Undefined variable '" + name + "'");
                            return;
                        }
                        *stackTop++ = v;
                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_GLOBAL) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        globalEnv->assign(name, *(stackTop - 1));
                    }
                    DISPATCH();
                }

                CASE_CODE(HAS_GLOBAL) {
                    uint16_t nameIdx = READ_SHORT();
                    const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                    *stackTop++ = Value(globalEnv->contains(name));
                    DISPATCH();
                }

                CASE_CODE(LOAD_GLOBAL_SLOT) {
                    uint16_t slot = READ_SHORT();
                    // Fast O(1) indexed array access Ã¢â‚¬â€ no mutex, no hash lookup
                    if (__builtin_expect(slot < globalSlots.size(), 1)) {
                        *stackTop++ = globalSlots[slot];
                    } else {
                        // Slot not yet initialized Ã¢â‚¬â€ should not happen in well-formed bytecode
                        *stackTop++ = Value();
                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_GLOBAL_SLOT) {
                    uint16_t slot = READ_SHORT();
                    // O(1) direct array write Ã¢â‚¬â€ no mutex, no hash
                    if (__builtin_expect(slot < globalSlots.size(), 1)) {
                        globalSlots[slot] = *(stackTop - 1);
                    }
                    DISPATCH();
                }

                // Ã¢â€â‚¬Ã¢â€â‚¬ Issue D: Fused loop-condition superinstructions Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
                // Replaces: LOAD_LOCAL i | LOAD_LOCAL end | LESS_EQ | JUMP_IF_FALSE exit
                // With one dispatch that reads locals directly Ã¢â‚¬â€ no stack push/pop.
                CASE_CODE(LOOP_LESS_EQ_LOCAL) {
                    uint8_t  loopSlot  = READ_BYTE();
                    uint8_t  endSlot   = READ_BYTE();
                    uint32_t exitOff   = READ_INT();
                    const Value& lv = frame->slots[loopSlot];
                    const Value& ev = frame->slots[endSlot];
                    // Integer fast path Ã¢â‚¬â€ covers 100% of repeat i=0 to N loops
                    if (__builtin_expect(lv.isInteger() && ev.isInteger(), 1)) {
                        if (lv.asInteger() > ev.asInteger()) ip += exitOff;
                    } else if (lv.isNumber() && ev.isNumber()) {
                        if (lv.asNumber() > ev.asNumber())  ip += exitOff;
                    } else {
                        ip += exitOff; // non-numeric: treat as loop-done
                    }
                    DISPATCH();
                }

                CASE_CODE(LOOP_GREATER_EQ_LOCAL) {
                    uint8_t  loopSlot  = READ_BYTE();
                    uint8_t  endSlot   = READ_BYTE();
                    uint32_t exitOff   = READ_INT();
                    const Value& lv = frame->slots[loopSlot];
                    const Value& ev = frame->slots[endSlot];
                    if (__builtin_expect(lv.isInteger() && ev.isInteger(), 1)) {
                        if (lv.asInteger() < ev.asInteger()) ip += exitOff;
                    } else if (lv.isNumber() && ev.isNumber()) {
                        if (lv.asNumber() < ev.asNumber())  ip += exitOff;
                    } else {
                        ip += exitOff;
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value obj = *(--stackTop);
                        if (obj.isInstance()) {
                            CHECK_VISIBILITY(obj.asInstance()->klass, propName);
                            Value val = obj.asInstance()->getProperty(propName);
                            if (val.isFunction() || val.isClosure() || val.isNativeFunction()) *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, val));
                            else *stackTop++ = val;
                        } else if (obj.isDictionary()) {
                            auto dictPtr = obj.asDictionaryPtr();
                            std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                            auto it = dictPtr->map.find(propName);
                            *stackTop++ = (it != dictPtr->map.end() ? it->second : Value());
                        } else if (obj.isClass()) {
                            auto klass = obj.asClass();
                            CHECK_VISIBILITY(klass, propName);
                            if (propName == "load" && klass->behaviors.persistent && !klass->persistPath.empty()) {
                                auto loadFn = [klass, this](RuntimeContext&, const std::vector<Value>&) -> Value {
                                    auto inst = std::make_shared<EZInstance>(klass);
                                    sqlite3* db = nullptr;
                                    auto it = this->persistDBConnections.find(klass->persistPath);
                                    if (it != this->persistDBConnections.end()) {
                                        db = static_cast<sqlite3*>(it->second);
                                    } else {
                                        if (sqlite3_open(klass->persistPath.c_str(), &db) == SQLITE_OK) {
                                            const char* create_sql = "CREATE TABLE IF NOT EXISTS EZ_Persist (prop TEXT PRIMARY KEY, val TEXT);";
                                            sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);
                                            this->persistDBConnections[klass->persistPath] = db;
                                        } else {
                                            db = nullptr;
                                        }
                                    }
                                    if (db) {
                                        sqlite3_stmt* stmt;
                                        if (sqlite3_prepare_v2(db, "SELECT prop, val FROM EZ_Persist;", -1, &stmt, nullptr) == SQLITE_OK) {
                                            while (sqlite3_step(stmt) == SQLITE_ROW) {
                                                const char* p = (const char*)sqlite3_column_text(stmt, 0);
                                                const char* v = (const char*)sqlite3_column_text(stmt, 1);
                                                std::string valStr(v);
                                                Value parsedVal;
                                                if (valStr == "true") parsedVal = Value(true);
                                                else if (valStr == "false") parsedVal = Value(false);
                                                else if (valStr == "nil") parsedVal = Value();
                                                else {
                                                    char* end;
                                                    double d = std::strtod(valStr.c_str(), &end);
                                                    // If the entire string was parsed as a number and it's not empty
                                                    if (!valStr.empty() && end == valStr.c_str() + valStr.length()) {
                                                        parsedVal = Value(d);
                                                    } else {
                                                        parsedVal = Value(valStr);
                                                    }
                                                }
                                                inst->setProperty(p, parsedVal);
                                            }
                                            sqlite3_finalize(stmt);
                                        }
                                    }
                                    return Value(inst);
                                };
                                *stackTop++ = Value(std::make_shared<NativeFunction>("load", 0, loadFn));
                            } else {
                                if (klass->staticMembers.count(propName)) *stackTop++ = klass->staticMembers[propName];
                                else if (klass->methods.count(propName)) *stackTop++ = klass->methods[propName];
                                else *stackTop++ = Value();
                            }
                        } else if (obj.isSuper()) {
                            auto super = obj.asSuper();
                            Value method = Value();
                            if (super->parentKlass->methods.count(propName)) {
                                method = super->parentKlass->methods[propName];
                            } else {
                                auto currentClass = super->parentKlass->parent;
                                while (currentClass) {
                                    if (currentClass->methods.count(propName)) {
                                        method = currentClass->methods[propName];
                                        break;
                                    }
                                    currentClass = currentClass->parent;
                                }
                            }
                            if (method.isFunction() || method.isClosure() || method.isNativeFunction()) *stackTop++ = Value(std::make_shared<EZBoundMethod>(Value(super->instance), method));
                            else *stackTop++ = method;
                        } else {
                            SYNC_IP();
                            runtimeError("Cannot access property '" + propName + "' on " + obj.typeName());
                            return;
                        }

                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value value = *(--stackTop);
                        Value obj   = *(--stackTop);
                        if (obj.isInstance()) {
                            CHECK_VISIBILITY(obj.asInstance()->klass, propName);
                            obj.asInstance()->setProperty(propName, value);
                        }
                        else if (obj.isClass()) {
                            CHECK_VISIBILITY(obj.asClass(), propName);
                            obj.asClass()->staticMembers[propName] = value;
                        }
                        else if (obj.isDictionary()) {
                            auto dictPtr = obj.asDictionaryPtr();
                            std::unique_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                            dictPtr->map[propName] = value;
                        }
                        else {
                            SYNC_IP();
                            runtimeError("Cannot set property '" + propName + "' on " + obj.typeName());
                            return;
                        }
                        *stackTop++ = value;
                    }
                    DISPATCH();
                }

                CASE_CODE(INTERCEPTED_STORE_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value value = *(--stackTop);
                        Value obj   = *(--stackTop);

                        if (!obj.isInstance()) {
                            if (obj.isClass()) {
                                CHECK_VISIBILITY(obj.asClass(), propName);
                                obj.asClass()->staticMembers[propName] = value;
                            } else if (obj.isDictionary()) {
                                auto dictPtr = obj.asDictionaryPtr();
                                std::unique_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                                dictPtr->map[propName] = value;
                            } else {
                                SYNC_IP();
                                runtimeError("Cannot set property '" + propName + "' on " + obj.typeName());
                                return;
                            }
                            *stackTop++ = value;
                        } else {
                            auto inst  = obj.asInstance();
                            auto klass = inst->klass;

                            if (!klass->behaviors.any()) {
                                CHECK_VISIBILITY(klass, propName);
                                inst->setProperty(propName, value);
                                *stackTop++ = value;
                            } else {
                                if (klass->behaviors.validated) {
                                    for (const auto& v : klass->validators) {
                                        if (v.field != propName) continue;
                                        bool ok = false;
                                        if (v.rule == "notnull") {
                                            ok = !value.isNil();
                                        } else if (v.rule == "minlen") {
                                            ok = value.isString() && (long long)value.asString().size() >= v.param.asInteger();
                                        } else if (v.rule == "maxlen") {
                                            ok = value.isString() && (long long)value.asString().size() <= v.param.asInteger();
                                        } else if (v.rule == "min") {
                                            ok = value.asFloat() >= v.param.asFloat();
                                        } else if (v.rule == "max") {
                                            ok = value.asFloat() <= v.param.asFloat();
                                        } else if (v.rule == "email") {
                                            const auto& s = value.isString() ? value.asString() : std::string();
                                            auto at = s.find('@');
                                            ok = (at != std::string::npos && at > 0 && at < s.size()-1 && s.find('.', at) != std::string::npos);
                                        } else if (v.rule == "pattern") {
                                            try { ok = std::regex_match(value.asString(), std::regex(v.param.asString())); } catch (...) { ok = false; }
                                        } else { ok = true; }
                                        if (!ok) {
                                            SYNC_IP();
                                            runtimeError("ValidationError: " + v.message + " (field '" + propName + "')");
                                            return;
                                        }
                                    }
                                }

                                Value oldValue;
                                if (klass->behaviors.audited || klass->behaviors.hasCached) {
                                    oldValue = inst->getProperty(propName);
                                }

                                CHECK_VISIBILITY(klass, propName);
                                inst->setProperty(propName, value);

                                if (klass->behaviors.audited) {
                                    if (!inst->auditLog) inst->auditLog = new std::vector<AuditEntry>();
                                    AuditEntry e;
                                    e.field     = propName;
                                    e.oldValue  = oldValue;
                                    e.newValue  = value;
                                    e.via       = frame->function->name;
                                    e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      std::chrono::system_clock::now().time_since_epoch()).count();
                                    inst->auditLog->push_back(std::move(e));
                                }

                                if (klass->behaviors.hasCached && inst->cacheStore) {
                                    for (auto& [methodName, cr] : *inst->cacheStore) {
                                        if (cr.deps.count(propName)) cr.dirty = true;
                                    }
                                }

                                if (klass->behaviors.persistent && !klass->persistPath.empty()) {
                                    sqlite3* db = nullptr;
                                    auto it = persistDBConnections.find(klass->persistPath);
                                    if (it != persistDBConnections.end()) {
                                        db = static_cast<sqlite3*>(it->second);
                                    } else {
                                        if (sqlite3_open(klass->persistPath.c_str(), &db) == SQLITE_OK) {
                                            const char* create_sql = "CREATE TABLE IF NOT EXISTS EZ_Persist (prop TEXT PRIMARY KEY, val TEXT);";
                                            sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);
                                            persistDBConnections[klass->persistPath] = db;
                                        } else {
                                            db = nullptr;
                                        }
                                    }
                                    
                                    if (db) {
                                        std::string valStr = value.toString();
                                        std::string sql = "INSERT OR REPLACE INTO EZ_Persist (prop, val) VALUES (?, ?);";
                                        sqlite3_stmt* stmt;
                                        if (sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr) == SQLITE_OK) {
                                            sqlite3_bind_text(stmt, 1, propName.c_str(), -1, SQLITE_TRANSIENT);
                                            sqlite3_bind_text(stmt, 2, valStr.c_str(), -1, SQLITE_TRANSIENT);
                                            sqlite3_step(stmt);
                                            sqlite3_finalize(stmt);
                                        }
                                    }
                                }

                                *stackTop++ = value;
                            }
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(RATELIMIT_CHECK) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& taskName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        // Stack: [key_string, count, per_string]
                        Value perVal   = *(--stackTop);
                        Value countVal = *(--stackTop);
                        Value keyVal   = *(--stackTop);
                        std::string key   = taskName + ":" + keyVal.toString();
                        long long maxCnt  = countVal.isInteger() ? countVal.asInteger() : (long long)countVal.asFloat();
                        std::string per   = perVal.toString();
                        long long windowMs = 60000LL;
                        if (per == "second") windowMs = 1000LL;
                        else if (per == "minute") windowMs = 60000LL;
                        else if (per == "hour") windowMs = 3600000LL;
                        else if (per == "day") windowMs = 86400000LL;
                        long long now = std::chrono::duration_cast<std::chrono::milliseconds>(
                                            std::chrono::system_clock::now().time_since_epoch()).count();
                        auto& win = rateLimiterRegistry[key];
                        while (!win.empty() && now - win.front() > windowMs) win.pop_front();
                        if ((long long)win.size() >= maxCnt) {
                            long long waitMs = windowMs - (now - win.front());
                            SYNC_IP();
                            runtimeError("RateLimitError: rate limit exceeded for '" + taskName + "'. Retry in " + std::to_string(waitMs) + "ms");
                            return;
                        }
                        win.push_back(now);
                    }
                    DISPATCH();
                }

                CASE_CODE(GET_CACHED_RESULT) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& methodName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value self = frame->slots[0]; // self is first slot
                        if (self.isInstance()) {
                            auto inst = self.asInstance();
                            if (inst->cacheStore) {
                                auto it = inst->cacheStore->find(methodName);
                                if (it != inst->cacheStore->end() && !it->second.dirty) {
                                    *stackTop++ = it->second.result;
                                } else {
                                    *stackTop++ = Value(); // nil = cache miss
                                }
                            } else {
                                *stackTop++ = Value(); // nil = cache miss
                            }
                        } else {
                            *stackTop++ = Value(); // nil = cache miss
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_CACHED_RESULT) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& methodName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value result = *(stackTop - 1); // peek, don't pop
                        Value self = frame->slots[0];
                        if (self.isInstance()) {
                            auto inst = self.asInstance();
                            if (!inst->cacheStore) inst->cacheStore = new std::unordered_map<std::string, CachedResult>();
                            auto& cr = (*inst->cacheStore)[methodName];
                            cr.result = result;
                            cr.dirty  = false;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(POP)  --stackTop; DISPATCH();
                CASE_CODE(DUP)  *stackTop = *(stackTop - 1); stackTop++; DISPATCH();
                CASE_CODE(DUP2) {
                    {
                        Value b = *(stackTop - 1), a = *(stackTop - 2); 
                        *stackTop++ = a; *stackTop++ = b;
                    }
                    DISPATCH(); 
                }

                CASE_CODE(LOAD_NIL)   *stackTop++ = Value(); DISPATCH();
                CASE_CODE(LOAD_TRUE)  *stackTop++ = Value(true); DISPATCH();
                CASE_CODE(LOAD_FALSE) *stackTop++ = Value(false); DISPATCH();
                CASE_CODE(LOAD_ZERO)  *stackTop++ = Value(0LL); DISPATCH();
                CASE_CODE(LOAD_ONE)   *stackTop++ = Value(1LL); DISPATCH();
                CASE_CODE(LOAD_EMPTY_STR) *stackTop++ = Value(""); DISPATCH();

                CASE_CODE(INC_LOCAL) {
                    Value& v = frame->slots[READ_BYTE()];
                    if (v.isInteger()) { // INTEGER
                        v = Value(v.asInteger() + 1LL);
                    } else if (v.isFloat()) { // NUMBER (double)
                        v = Value(v.asFloat() + 1.0);
                    } else {
                        v = Value(v.asNumber() + 1.0);
                    }
                    DISPATCH();
                }
                CASE_CODE(DEC_LOCAL) {
                    Value& v = frame->slots[READ_BYTE()];
                    if (v.isInteger()) { // INTEGER
                        v = Value(v.asInteger() - 1LL);
                    } else if (v.isFloat()) { // NUMBER (double)
                        v = Value(v.asFloat() - 1.0);
                    } else {
                        v = Value(v.asNumber() - 1.0);
                    }
                    DISPATCH();
                }

                CASE_CODE(NEGATE) {
                    {
                        Value& v = stackTop[-1];
                        if (v.isInteger()) {
                            v = Value(-v.asInteger());
                        } else if (v.isFloat()) {
                            v = Value(-v.asFloat());
                        } else if (v.isInstance()) {
                            bool handled = false;
                            {
                                Value method = v.asInstance()->getProperty("neg");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-1] = Value(std::make_shared<EZBoundMethod>(v, method));
                                    if (!dispatchCall(stackTop[-1], 0)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doNegate(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doNegate(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(ADD) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            long long res = a.asInteger() + b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            double res = a.asNumber() + b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("+");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doAdd(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doAdd(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(SUB) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            long long res = a.asInteger() - b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            double res = a.asNumber() - b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("-");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doSubtract(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doSubtract(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(MUL) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            long long res = a.asInteger() * b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            double res = a.asNumber() * b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("*");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doMultiply(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doMultiply(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(DIV) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            long long al = a.asInteger();
                            long long bl = b.asInteger();
                            if (bl == 0) { SYNC_IP(); runtimeError("Division by zero"); return; }
                            
                            if (al % bl == 0) {
                                long long res = al / bl;
                                stackTop -= 2;
                                *stackTop = Value(res);
                                stackTop++;
                            } else {
                                double res = static_cast<double>(al) / static_cast<double>(bl);
                                stackTop -= 2;
                                *stackTop = Value(res);
                                stackTop++;
                            }
                        } else if (a.isNumber() && b.isNumber()) {
                            double db = b.asNumber();
                            if (db == 0) { SYNC_IP(); runtimeError("Division by zero"); return; }
                            double res = a.asNumber() / db;
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("/");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doDivide(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doDivide(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(MOD) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            long long bl = b.asInteger();
                            if (bl == 0) { SYNC_IP(); runtimeError("Modulo by zero"); return; }
                            long long res = a.asInteger() % bl;
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doModulo(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(POW) { SYNC_IP(); this->stackTop = stackTop; doPower(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_AND)    { SYNC_IP(); this->stackTop = stackTop; doBitwiseAnd(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_OR)     { SYNC_IP(); this->stackTop = stackTop; doBitwiseOr(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_XOR)    { SYNC_IP(); this->stackTop = stackTop; doBitwiseXor(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_NOT)    { SYNC_IP(); this->stackTop = stackTop; doBitwiseNot(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(SHIFT_LEFT) { SYNC_IP(); this->stackTop = stackTop; doShiftLeft(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(SHIFT_RIGHT){ SYNC_IP(); this->stackTop = stackTop; doShiftRight(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }

                CASE_CODE(EQUAL) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.m_data.index() == b.m_data.index()) {
                            bool res;
                            if (a.isInteger()) res = (a.asInteger() == b.asInteger());
                            else if (a.isFloat()) res = (a.asFloat() == b.asFloat());
                            else if (a.isBool()) res = (a.asBool() == b.asBool());
                            else if (a.isNil()) res = true;
                            else { SYNC_IP(); this->stackTop = stackTop; doEqual(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("==");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doEqual(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doEqual(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(NOT_EQUAL) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("!=");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doNotEqual(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doNotEqual(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(LESS) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            bool res = a.asInteger() < b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            bool res = a.asNumber() < b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("<");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doLess(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doLess(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(LESS_EQ) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            bool res = a.asInteger() <= b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            bool res = a.asNumber() <= b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty("<=");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doLessEq(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doLessEq(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(GREATER) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            bool res = a.asInteger() > b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            bool res = a.asNumber() > b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty(">");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doGreater(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doGreater(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(GREATER_EQ) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            bool res = a.asInteger() >= b.asInteger();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isNumber() && b.isNumber()) {
                            bool res = a.asNumber() >= b.asNumber();
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
                            bool handled = false;
                            {
                                Value method = a.asInstance()->getProperty(">=");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    stackTop[-2] = Value(std::make_shared<EZBoundMethod>(a, method));
                                    if (!dispatchCall(stackTop[-2], 1)) return;
                                    handled = true;
                                }
                            }
                            if (handled) {
                                LOAD_FRAME();
                                DISPATCH();
                            }
                            SYNC_IP(); this->stackTop = stackTop; doGreaterEq(); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; doGreaterEq(); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(NOT)        { SYNC_IP(); this->stackTop = stackTop; doNot(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }

                CASE_CODE(JUMP)           ip += READ_INT(); DISPATCH();
                CASE_CODE(JUMP_IF_FALSE) {
                    {
                        uint32_t o = READ_INT(); 
                        const Value& v = *(--stackTop);
                        bool truthy;
                        if (v.isBool()) truthy = v.asBool();
                        else if (v.isNil()) truthy = false;
                        else truthy = v.isTruthy();
                        if (!truthy) ip += o;
                    }
                    DISPATCH(); 
                }
                CASE_CODE(JUMP_IF_TRUE) {
                    {
                        uint32_t o = READ_INT(); 
                        const Value& v = *(--stackTop);
                        bool truthy;
                        if (v.isBool()) truthy = v.asBool();
                        else if (v.isNil()) truthy = false;
                        else truthy = v.isTruthy();
                        if (truthy) ip += o;
                    }
                    DISPATCH(); 
                }
                CASE_CODE(JUMP_IF_NIL) {
                    {
                        uint32_t o = READ_INT(); 
                        const Value& v = *(--stackTop);
                        if (v.isNil()) ip += o;
                    }
                    DISPATCH(); 
                }
                CASE_CODE(JUMP_IF_NOT_NIL) {
                    {
                        uint32_t o = READ_INT(); 
                        const Value& v = *(--stackTop);
                        if (!v.isNil()) ip += o;
                    }
                    DISPATCH(); 
                }
                CASE_CODE(LOOP)           ip -= READ_INT();  DISPATCH();

                CASE_CODE(CALL) {
                    bool handled = false;
                    {
                        uint8_t argCount = READ_BYTE();
                        Value callee = *(stackTop - argCount - 1);
                        
                        if (callee.isNativeFunction() && callee.asNativeFunction()->name == "str" && argCount == 1) {
                            Value arg = *(stackTop - 1);
                            if (arg.isInstance()) {
                                Value method = arg.asInstance()->getProperty("toString");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    this->stackTop = stackTop;
                                    Value bound = Value(std::make_shared<EZBoundMethod>(arg, method));
                                    *(stackTop - 2) = bound;
                                    stackTop--; // remove arg to leave 0 args
                                    if (dispatchCall(bound, 0)) {
                                        LOAD_FRAME();
                                    } else {
                                        REFRESH_FRAME();
                                    }
                                    stackTop = this->stackTop;
                                    handled = true;
                                }
                            }
                        }

                        if (!handled) {
                            SYNC_IP();
                            this->stackTop = stackTop;
                            if (dispatchCall(callee, argCount)) {
                                LOAD_FRAME();
                            } else {
                                REFRESH_FRAME();
                            }
                            stackTop = this->stackTop;
                        }
                    }
                    DISPATCH();
                }
                
                CASE_CODE(CALL_KW) {
                    bool handled = false;
                    {
                        uint8_t posCount = READ_BYTE();
                        Value kwargs = *(--stackTop); // Pop kwargs dictionary
                        Value callee = *(stackTop - posCount - 1);
                        
                        uint8_t totalArity = posCount;
                        
                        if (callee.isNativeFunction()) {
                            // Can't map, just append kwargs as last positional arg
                            *stackTop++ = kwargs;
                            totalArity = posCount + 1;
                        } else {
                            size_t paramOffset = 0;
                            Value target = callee;
                            if (target.isClass()) {
                                auto klass = target.asClass();
                                if (klass->methods.count("init")) target = klass->methods["init"];
                                else target = Value();
                                paramOffset = 1;
                            } else if (target.isBoundMethod()) {
                                target = target.asBoundMethod()->method;
                                paramOffset = 1;
                            } else if (target.isSuper()) {
                                auto super_val = target.asSuper();
                                if (super_val->parentKlass->methods.count("init")) target = super_val->parentKlass->methods.at("init");
                                else target = Value();
                                paramOffset = 1;
                            }
                            
                            std::vector<std::string> paramNames;
                            if (target.isFunction()) {
                                paramNames = target.asFunction()->params;
                            } else if (target.isClosure()) {
                                paramNames = target.asClosure()->function->paramNames;
                            }
                            
                            // Check if the target actually has parameters and if paramOffset is valid
                            if (paramOffset > 0 && (paramNames.empty() || paramNames[0] != "self")) {
                                paramOffset = 0; // fallback if it doesn't have 'self'
                            }
                            
                            size_t expectedCallerParams = paramNames.size() > paramOffset ? paramNames.size() - paramOffset : 0;
                            
                            if (expectedCallerParams > posCount) {
                                auto dict = kwargs.asDictionaryPtr();
                                std::unique_lock<std::shared_mutex> lk(dict->map_mutex);
                                
                                std::vector<Value> posArgs(stackTop - posCount, stackTop);
                                stackTop -= posCount;
                                
                                for (size_t i = paramOffset; i < paramNames.size(); ++i) {
                                    size_t callerArgIdx = i - paramOffset;
                                    if (callerArgIdx < posArgs.size()) {
                                        *stackTop++ = posArgs[callerArgIdx];
                                    } else {
                                        auto it = dict->map.find(paramNames[i]);
                                        if (it != dict->map.end()) {
                                            *stackTop++ = it->second;
                                            dict->map.erase(it);
                                        } else {
                                            *stackTop++ = Value(); // Missing arg, will be handled by defaults
                                        }
                                    }
                                }
                                
                                if (!dict->map.empty()) {
                                    std::string unexpected = dict->map.begin()->first;
                                    SYNC_IP();
                                    runtimeError("Unexpected keyword argument '" + unexpected + "'");
                                }
                                totalArity = expectedCallerParams;
                            } else {
                                if (!kwargs.asDictionaryPtr()->map.empty()) {
                                    SYNC_IP();
                                    runtimeError("Unexpected keyword argument");
                                }
                            }
                        }
                        
                        if (!handled) {
                            SYNC_IP();
                            this->stackTop = stackTop;
                            if (dispatchCall(callee, totalArity)) {
                                LOAD_FRAME();
                            } else {
                                REFRESH_FRAME();
                            }
                            stackTop = this->stackTop;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(TAIL_CALL) {
                    {
                        uint8_t argCount = READ_BYTE();
                        Value callee = *(stackTop - argCount - 1);
                        
                        if (callee.isFunction()) {
                            auto ezFunc = callee.asFunction();
                            auto it = compiledFunctionCache.find(ezFunc.get());
                            if (it != compiledFunctionCache.end() && it->second == frame->function) {
                                for (int i = 0; i < argCount; i++) {
                                    frame->slots[i] = *(stackTop - argCount + i);
                                }
                                stackTop = frame->slots + frame->function->localCount;
                                ip = frame->function->chunk.code.data();
                                // callee/ezFunc destroyed at scope exit below before DISPATCH
                                goto tail_call_restart;
                            }
                        }
                        
                        SYNC_IP();
                        this->stackTop = stackTop;
                        if (dispatchCall(callee, argCount)) {
                            LOAD_FRAME();
                        } else {
                            REFRESH_FRAME();
                        }
                        stackTop = this->stackTop;
                    }
                    DISPATCH();
                    tail_call_restart:
                    DISPATCH();
                }

                CASE_CODE(RETURN) {
                    {
                        Value result = *(--stackTop);
                        SYNC_IP();
                        Value* targetSlots = frame->slots;
                        closeUpvalues(targetSlots);
                        frameUpvalues.pop_back();
                        frames.pop_back();
                        
                        if (frames.size() < startingFrameCount) {
                            this->stackTop = targetSlots - 1;
                            *(this->stackTop) = result;
                            this->stackTop++;
                            return;
                        }
                        
                        LOAD_FRAME();
                        stackTop = targetSlots - 1; // Replace callee with result
                        *stackTop++ = result;
                    }
                    DISPATCH();
                }

                CASE_CODE(CLOSE_UPVALUE) {
                    uint8_t slot = READ_BYTE();
                    this->stackTop = stackTop;
                    closeUpvalues(frame->slots + slot);
                    stackTop = this->stackTop;
                    DISPATCH();
                }

                CASE_CODE(MAKE_ARRAY) {
                    {
                        uint8_t count = READ_BYTE();
                        std::vector<Value> el(count);
                        for (int i = count - 1; i >= 0; i--) el[i] = *(--stackTop);
                        *stackTop++ = Value::makeArray(el);
                    }
                    DISPATCH();
                }

                CASE_CODE(BUILD_TUPLE) {
                    {
                        uint8_t count = READ_BYTE();
                        std::vector<Value> el(count);
                        for (int i = count - 1; i >= 0; i--) el[i] = *(--stackTop);
                        *stackTop++ = Value::makeTuple(el);
                    }
                    DISPATCH();
                }

                CASE_CODE(MAKE_DICT) {
                    {
                        uint8_t pairs = READ_BYTE();
                        Value dict = Value::makeDictionary();
                        auto& m = dict.asDictionaryPtr()->map;
                        for (int i = 0; i < pairs; i++) {
                            Value val = *(--stackTop);
                            Value key = *(--stackTop);
                            m[key.toString()] = val;
                        }
                        *stackTop++ = dict;
                    }
                    DISPATCH();
                }

                CASE_CODE(INDEX_GET)  { SYNC_IP(); this->stackTop = stackTop; doIndexGet(); stackTop = this->stackTop; DISPATCH(); }
                CASE_CODE(INDEX_SET)  { SYNC_IP(); this->stackTop = stackTop; doIndexSet(); stackTop = this->stackTop; DISPATCH(); }

                CASE_CODE(ARRAY_APPEND) {
                    {
                        Value val = *(--stackTop);
                        Value arr = *(stackTop - 1);
                        if (arr.isArray()) {
                            arr.asArray().push_back(val);
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(ARRAY_EXTEND) {
                    {
                        Value iter = *(--stackTop);
                        Value arr = *(stackTop - 1);
                        if (arr.isArray() && iter.isArray()) {
                            auto& src = iter.asArray();
                            auto& dst = arr.asArray();
                            for (const Value& v : src) {
                                dst.push_back(v);
                            }
                        } else if (arr.isArray() && iter.isString()) {
                            auto& dst = arr.asArray();
                            std::string s = iter.asString();
                            for (char c : s) {
                                dst.push_back(Value(std::string(1, c)));
                            }
                        } else {
                            SYNC_IP();
                            runtimeError("Cannot spread non-iterable value");
                            return;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(CALL_SPREAD) {
                    {
                        Value argsArray = *(--stackTop);
                        Value callee = *(stackTop - 1);
                        if (!argsArray.isArray()) {
                            SYNC_IP();
                            runtimeError("CALL_SPREAD requires an array of arguments");
                            return;
                        }
                        auto& args = argsArray.asArray();
                        int argc = args.size();
                        
                        // Recalculate callee after popping argsArray
                        callee = *(stackTop - 1);
                        
                        // Push all arguments to the stack
                        for (const Value& arg : args) {
                            *stackTop++ = arg;
                        }
                        
                        // Dispatch the call normally
                        SYNC_IP();
                        if (dispatchCall(callee, argc)) {
                            LOAD_FRAME();
                        } else {
                            REFRESH_FRAME();
                        }
                        stackTop = this->stackTop;
                    }
                    DISPATCH();
                }

                CASE_CODE(NEW_INSTANCE) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint8_t argc = READ_BYTE();
                        const std::string& className = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value classVal = globalEnv->get(className, frame->line);
                        if (!classVal.isClass()) {
                            SYNC_IP();
                            runtimeError("'" + className + "' is not a model");
                            this->stackTop = stackTop;
                            return;
                        }
                        std::vector<Value> args(argc);
                        for (int i = argc - 1; i >= 0; i--) args[i] = *(--stackTop);
                        
                        SYNC_IP();
                        this->stackTop = stackTop;
                        Value inst = instantiate(classVal.asClass(), args, frame->line, frame->functionName);
                        LOAD_FRAME();
                        stackTop = this->stackTop;
                        *stackTop++ = inst;
                    }
                    DISPATCH();
                }

                CASE_CODE(SUPER) {
                    {
                        Value parent = *(--stackTop), inst = *(--stackTop);
                        if (!inst.isInstance() || !parent.isClass()) {
                            SYNC_IP();
                            runtimeError("'super' must be used with model instance");
                            this->stackTop = stackTop;
                            return;
                        }
                        *stackTop++ = Value::makeSuper(inst.asInstance(), parent.asClass());
                    }
                    DISPATCH();
                }
                
                CASE_CODE(SUPER_CALL) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint8_t argCount = READ_BYTE();
                        const std::string& method = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value superVal = *(--stackTop); 
                        if (!superVal.isSuper()) {
                            SYNC_IP();
                            runtimeError("SUPER_CALL expects super object");
                            this->stackTop = stackTop;
                            return;
                        }
                        auto super = superVal.asSuper();
                        Value m = Value();
                        auto currentClass = super->parentKlass;
                        while (currentClass) {
                            if (currentClass->methods.count(method)) {
                                m = currentClass->methods[method];
                                break;
                            }
                            currentClass = currentClass->parent;
                        }
                        if (m.isNil()) {
                            SYNC_IP();
                            runtimeError("Method '" + method + "' not found in superclass");
                            this->stackTop = stackTop;
                            return;
                        }
                        Value rec = Value(super->instance);
                        Value bound = Value(std::make_shared<EZBoundMethod>(rec, m));
                        
                        // Shift args right to make room for bound method
                        push(Value()); 
                        for (int i = 0; i < argCount; i++) {
                            *(stackTop - i - 1) = *(stackTop - i - 2);
                        }
                        *(stackTop - argCount - 1) = bound;
                        
                        SYNC_IP();
                        this->stackTop = stackTop;
                        if (!dispatchCall(m, argCount + 1)) return;
                        LOAD_FRAME();
                        stackTop = this->stackTop;
                    }
                    DISPATCH();
                }

                CASE_CODE(GET_ITER) {
                    {
                        Value v = *(--stackTop);
                        if (v.isArray()) {
                            *stackTop++ = Value::makeArray({v, Value(0LL)});
                        } else if (v.isString()) {
                            std::vector<Value> cs;
                            for (char c : v.asString()) cs.push_back(Value(std::string(1, c)));
                            *stackTop++ = Value::makeArray({Value::makeArray(cs), Value(0LL)});
                        } else if (v.isDictionary()) {
                            std::vector<Value> ks;
                            {
                                auto dictPtr = v.asDictionaryPtr();
                                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                                for (auto& [k, _] : dictPtr->map) ks.push_back(Value(k));
                            }
                            *stackTop++ = Value::makeArray({Value::makeArray(ks), Value(0LL)});
                        } else {
                            SYNC_IP();
                            runtimeError("Not iterable: " + v.typeName());
                            return;
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(GET_DICT_ITER) {
                    {
                        Value v = *(--stackTop);
                        if (v.isDictionary()) {
                            std::vector<Value> pairs;
                            {
                                auto dictPtr = v.asDictionaryPtr();
                                std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
                                for (auto& [k, val] : dictPtr->map) {
                                    pairs.push_back(Value::makeArray({Value(k), val}));
                                }
                            }
                            *stackTop++ = Value::makeArray({Value::makeArray(pairs), Value(0LL)});
                        } else {
                            SYNC_IP();
                            runtimeError("Not a dictionary: " + v.typeName());
                            return;
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(ITER_NEXT) {
                    {
                        uint32_t offset = READ_INT();
                        Value& iter = *(stackTop - 1);
                        auto& iArr = iter.asArray();
                        long long idx = iArr[1].asInteger();
                        const auto& data = iArr[0].asArray();
                        if (idx >= (long long)data.size()) { --stackTop; ip += offset; }
                        else { 
                            iArr[1] = Value(idx + 1); 
                            *(stackTop - 1) = data[idx]; 
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(ITER_HAS_NEXT) {
                    {
                        Value& iter = *(stackTop - 1);
                        auto& iArr = iter.asArray();
                        long long idx = iArr[1].asInteger();
                        const auto& data = iArr[0].asArray();
                        *stackTop++ = Value(idx < (long long)data.size());
                    }
                    DISPATCH();
                }
                
                CASE_CODE(TO_STRING) {
                    bool handled = false;
                    {
                        Value v = *(stackTop - 1);
                        if (v.isInstance()) {
                            Value method = v.asInstance()->getProperty("toString");
                            if (method.isCallable()) {
                                SYNC_IP();
                                this->stackTop = stackTop;
                                Value bound = Value(std::make_shared<EZBoundMethod>(v, method));
                                *(stackTop - 1) = bound;
                                if (dispatchCall(bound, 0)) {
                                    LOAD_FRAME();
                                } else {
                                    REFRESH_FRAME();
                                }
                                stackTop = this->stackTop;
                                handled = true;
                            }
                        }
                        if (!handled) {
                            *(stackTop - 1) = Value(v.toString());
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(PRINT) {
                    {
                        Value v = *(--stackTop);
                        std::cout << v.toString() << std::endl;
                    }
                    DISPATCH();
                }
                CASE_CODE(CLOCK) { 
                    auto now = std::chrono::steady_clock::now().time_since_epoch();
                    *stackTop++ = Value(std::chrono::duration<double>(now).count() * 1000.0);
                    DISPATCH(); 
                }
                CASE_CODE(TYPE_OF) {
                    {
                        Value v = *(--stackTop);
                        *stackTop++ = Value(v.typeName());
                    }
                    DISPATCH();
                }
                CASE_CODE(IS_INSTANCE_OF) {
                    {
                        std::string className = (*(--stackTop)).toString();
                        Value v = *(--stackTop);
                        bool result = false;
                        
                        if (className == "Number") {
                            result = v.isInteger() || v.isFloat();
                        } else if (className == "Boolean") {
                            result = v.isBool();
                        } else if (className == "Integer") {
                            result = v.isInteger();
                        } else if (className == "Float") {
                            result = v.isFloat();
                        } else if (className == "String") {
                            result = v.isString();
                        } else if (className == "Array") {
                            result = v.isArray();
                        } else if (className == "Dictionary") {
                            result = v.isDictionary();
                        } else if (className == "Function") {
                            result = v.isFunction() || v.isNativeFunction() || v.isBoundMethod() || v.isClosure();
                        } else if (className == "Nil") {
                            result = v.isNil();
                        } else if (v.isInstance()) {
                            auto inst = v.asInstance();
                            auto currentClass = inst->klass;
                            while (currentClass) {
                                if (currentClass->name == className) {
                                    result = true;
                                    break;
                                }
                                currentClass = currentClass->parent;
                            }
                        }
                        *stackTop++ = Value(result);
                    }
                    DISPATCH();
                }

                CASE_CODE(OP_AWAIT) {
                    {
                        Value futVal = *(stackTop - 1);
                        if (futVal.isFuture()) {
                            auto fut = futVal.asFuture();
                            if (fut->isReady()) {
                                *(stackTop - 1) = fut->get();
                            } else {
                                SYNC_IP();
                                this->isYielded = true;
                                this->stackTop = stackTop;
                                
                                std::shared_ptr<BytecodeVM> sharedVM;
                                try {
                                    sharedVM = this->shared_from_this();
                                } catch (const std::bad_weak_ptr&) {
                                    // Main VM is stack allocated and kept alive by main()
                                }
                                BytecodeVM* rawVM = this;

                                fut->then([sharedVM, rawVM, fut]() {
                                    EventLoop::instance().pushTask([sharedVM, rawVM, fut]() {
                                        rawVM->isYielded = false;
                                        if (fut->isError()) {
                                            // Re-execute OP_AWAIT so it throws from inside the interpret loop
                                            if (!rawVM->frames.empty()) {
                                                rawVM->frames.back().ip -= 1;
                                            }
                                        } else {
                                            *(rawVM->stackTop - 1) = fut->get();
                                        }
                                        rawVM->run(0);
                                        
                                        if (!rawVM->isYielded && rawVM->taskFuture) {
                                            if (!rawVM->running && !rawVM->frames.empty()) {
                                                std::string errMsg = rawVM->pendingException.isString() ? rawVM->pendingException.toString() : "Async task failed with an exception";
                                                rawVM->taskFuture->setError(errMsg);
                                            } else {
                                                Value result = (rawVM->stackTop > rawVM->stack.data()) ? *(rawVM->stackTop - 1) : Value();
                                                rawVM->taskFuture->set(result);
                                            }
                                        }
                                    });
                                });
                                return;
                            }
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(MAKE_INTERFACE) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint8_t methodCount = READ_BYTE();
                        const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        
                        std::vector<std::string> methods;
                        for (int i = 0; i < methodCount; i++) {
                            methods.push_back((*(--stackTop)).toString());
                        }
                        std::reverse(methods.begin(), methods.end());
                        
                        auto iface = std::make_shared<EZInterface>(name, methods);
                        *stackTop++ = Value(iface);
                    }
                    DISPATCH();
                }

                CASE_CODE(MAKE_CLASS) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint8_t memberCount = READ_BYTE();
                        uint8_t interfaceCount = READ_BYTE();
                        uint8_t validatorCount = READ_BYTE();  // NEW
                        const std::string& className = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        auto klass = std::make_shared<EZClass>(className);
                        
                        // 1. Pop behavior metadata (pushed last, so pop first)
                        // Pop validators: each is [field, rule, param, message]
                        for (int i = 0; i < validatorCount; i++) {
                            FieldValidator fv;
                            fv.message = (*(--stackTop)).toString();
                            fv.param   = *(--stackTop);
                            fv.rule    = (*(--stackTop)).toString();
                            fv.field   = (*(--stackTop)).toString();
                            klass->validators.push_back(std::move(fv));
                        }
                        if (validatorCount > 0) klass->behaviors.validated = true;
                        
                        // Pop persist path
                        klass->persistPath = (*(--stackTop)).toString();
                        klass->behaviors.persistent = !klass->persistPath.empty();
                        
                        // Pop snapshot flag
                        klass->behaviors.snapshot = (*(--stackTop)).asBool();
                        
                        // Pop audited flag
                        klass->behaviors.audited = (*(--stackTop)).asBool();

                        // 2. Pop Interfaces
                        std::vector<std::shared_ptr<EZInterface>> interfaces;
                        for (int i = 0; i < interfaceCount; i++) {
                            Value v = *(--stackTop);
                            if (v.type() == ValueType::INTERFACE) {
                                interfaces.push_back(std::get<std::shared_ptr<EZInterface>>(v.m_data));
                            }
                        }

                        // 3. Pop Members
                        for (int i = 0; i < memberCount; i++) {
                            Value isPublicVal = *(--stackTop);
                            Value isStaticVal = *(--stackTop);
                            Value val = *(--stackTop);
                            Value key = *(--stackTop);
                            bool isPublic = isPublicVal.asBool();
                            bool isStatic = isStaticVal.asBool();
                            std::string memberName = key.toString();
                            
                            // Track visibility
                            klass->visibility[memberName] = isPublic;
                            
                            if (val.isClosure()) {
                                // Bind the method to this class for runtime access control
                                val.asClosure()->function->className = className;
                            }
                            
                            if (isStatic) {
                                klass->staticMembers[memberName] = val;
                            } else {
                                klass->methods[memberName] = val;
                            }
                        }

                        // 4. Pop Parent class (pushed first, so pop last)
                        Value parentVal = *(--stackTop);

                        if (parentVal.isClass()) {
                            klass->parent = parentVal.asClass();
                            // Inherit methods from parent
                            for (auto& [name, method] : klass->parent->methods) {
                                if (klass->methods.find(name) == klass->methods.end()) {
                                    klass->methods[name] = method;
                                }
                            }
                            // Inherit visibility from parent
                            for (auto& [name, isPublic] : klass->parent->visibility) {
                                if (klass->visibility.find(name) == klass->visibility.end()) {
                                    klass->visibility[name] = isPublic;
                                }
                            }
                        }

                        // Validate interfaces
                        for (const auto& iface : interfaces) {
                            for (const auto& methodName : iface->requiredMethods) {
                                if (klass->methods.find(methodName) == klass->methods.end()) {
                                    SYNC_IP();
                                    runtimeError("Model '" + className + "' fails to implement interface '" + 
                                                 iface->name + "': missing task '" + methodName + "'");
                                    return;
                                }
                            }
                        }

                        globalEnv->define(className, Value(klass));
                        *stackTop++ = Value(klass);
                    }
                    DISPATCH();
                }
                CASE_CODE(LINE)      frame->line = READ_SHORT(); DISPATCH();
                CASE_CODE(BREAKPOINT) SYNC_IP(); /* debug hook */ DISPATCH();

                CASE_CODE(GET_METHOD) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& method = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value instVal = *(stackTop - 1);
                        if (instVal.isInstance()) {
                            auto inst = instVal.asInstance();
                            CHECK_VISIBILITY(inst->klass, method);
                            Value m = inst->klass->methods.count(method) ? inst->klass->methods.at(method) : Value();
                            --stackTop;
                            if (m.isNil()) *stackTop++ = Value();
                            else if (m.isFunction() || m.isClosure() || m.isNativeFunction()) *stackTop++ = Value(std::make_shared<EZBoundMethod>(instVal, m));
                            else *stackTop++ = m;
                        } else {
                            SYNC_IP();
                            runtimeError("Cannot get method from non-instance");
                            this->stackTop = stackTop;
                            return;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(CLOSURE) {
                    {
                        uint16_t nestedIdx = READ_SHORT();
                        BytecodeFunctionPtr func = frame->function->nestedFunctions[nestedIdx];
                        auto closure = std::make_shared<EZClosure>(func);
                        for (int i = 0; i < (int)func->upvalues.size(); i++) {
                            uint8_t isLocal = READ_BYTE();
                            uint8_t index = READ_BYTE();
                            if (isLocal) {
                                closure->upvalues.push_back(captureUpvalue(frame->slots + index));
                            } else {
                                // index is into the enclosing closure's upvalue list
                                closure->upvalues.push_back(frameUpvalues.back().upvalues[index]);
                            }
                        }
                        *stackTop++ = Value(closure);
                    }
                    DISPATCH();
                }

                CASE_CODE(TRY_START) {
                    uint32_t offset = READ_INT();
                    TryBlock tb;
                    tb.frameIdx = frames.size() - 1;
                    tb.catchIp = ip + offset;
                    tb.stackTop = stackTop;
                    tryStack.push_back(tb);
                    DISPATCH();
                }

                CASE_CODE(TRY_END) {
                    if (!tryStack.empty()) tryStack.pop_back();
                    DISPATCH();
                }

                CASE_CODE(THROW) {
                    {
                        Value exc = *(--stackTop);
                        
                        if (exc.isDictionary()) {
                            auto dict = exc.asDictionaryPtr();
                            if (dict->map.count("stackTrace")) {
                                std::string st = "";
                                for (auto it = frames.rbegin(); it != frames.rend(); ++it) {
                                    int currentLine = it->line;
                                    if (it->function && !it->function->chunk.lines.empty()) {
                                        size_t offset = (size_t)(it->ip - it->function->chunk.code.data());
                                        if (offset > 0) offset--;
                                        if (offset < it->function->chunk.lines.size()) {
                                            currentLine = (int)it->function->chunk.lines[offset];
                                        }
                                    }
                                    std::string fn = it->functionName.empty() ? "<script>" : it->functionName;
                                    st += "  File \"" + (it->filename.empty() ? "<unknown>" : it->filename) +
                                          "\", line " + std::to_string(currentLine) +
                                          ", in " + fn + "\n";
                                }
                                dict->map["stackTrace"] = Value(st);
                            }
                        }
                        
                        pendingException = exc;
                        if (!tryStack.empty()) {
                            TryBlock tb = tryStack.back(); tryStack.pop_back();
                            while (frames.size() > tb.frameIdx + 1) {
                                closeUpvalues(frames.back().slots);
                                frames.pop_back(); frameUpvalues.pop_back();
                            }
                            stackTop = tb.stackTop;
                            LOAD_FRAME();
                            ip = tb.catchIp;
                            *stackTop++ = exc;
                        } else {
                            SYNC_IP();
                            if (exc.isDictionary() && exc.asDictionaryPtr()->map.count("message")) {
                                runtimeError("Uncaught exception: " + exc.asDictionaryPtr()->map["message"].toString());
                            } else {
                                runtimeError("Uncaught exception: " + exc.toString());
                            }
                            return;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(END) {
                    running = false;
#ifdef __GNUC__
                    goto end_run;
#else
                    break;
#endif
                }
            } // closes switch body
#ifndef __GNUC__
            } // closes while loop body from INTERPRET_LOOP
#endif

#ifdef __GNUC__
            end_run:
            SYNC_IP();
            this->stackTop = stackTop;
#endif

    } catch (const RuntimeError& e) {
        if (!tryStack.empty()) {
            TryBlock tb = tryStack.back(); tryStack.pop_back();
            while (frames.size() > tb.frameIdx + 1) {
                closeUpvalues(frames.back().slots);
                frames.pop_back(); frameUpvalues.pop_back();
            }
            this->stackTop = tb.stackTop;
            LOAD_FRAME();
            ip = tb.catchIp;
            *stackTop++ = e.value.isNil() ? Value(e.what()) : e.value;
            pendingException = Value(); // Clear pending exception after catch
            running = true;
            // Invariant: LOAD_FRAME() refreshes local stackTop and ip pointers from the restored CallFrame.
            // this->stackTop is restored to the exact level it was at the start of the 'try' block.
            goto dispatch_start;
        }
        // Uncaught RuntimeError: already printed in runtimeError() (if not async task)
        pendingException = e.value.isNil() ? Value(e.what()) : e.value;
        SYNC_IP();
    } catch (const std::exception& e) {
        if (!tryStack.empty()) {
            TryBlock tb = tryStack.back(); tryStack.pop_back();
            while (frames.size() > tb.frameIdx + 1) {
                closeUpvalues(frames.back().slots);
                frames.pop_back(); frameUpvalues.pop_back();
            }
            this->stackTop = tb.stackTop;
            LOAD_FRAME();
            ip = tb.catchIp;
            *stackTop++ = Value(e.what());
            pendingException = Value(); // Clear pending exception after catch
            running = true;
            goto dispatch_start;
        }
        // Uncaught std exception Ã¢â‚¬â€ format like our runtime errors
        pendingException = Value(e.what());
        if (!isAsyncTask) {
            std::string fname = frames.empty() ? "" : frames.back().filename;
            int ln = frames.empty() ? 0 : frames.back().line;
            std::cerr << "\nError: " << e.what() << std::endl;
            if (ln > 0) std::cerr << "  at line " << ln;
            if (!fname.empty()) std::cerr << " in " << fname;
            if (ln > 0 || !fname.empty()) std::cerr << std::endl;
            printStackTrace();
        }
        running = false;
        SYNC_IP();
    }
}

#undef READ_CONST

// ============================================================================
// Call Dispatch Helper
// ============================================================================

bool BytecodeVM::dispatchCall(const Value& callee, uint8_t argCount, bool bypassAsyncCheck) {
    if (callee.isNativeFunction()) {
        // Collect args from stack (they sit above the callee)
        std::vector<Value> args(stackTop - argCount, stackTop);
        
        try {
            Value result = callee.asNativeFunction()->function(*this, args);
            // NOW we pop everything and push the result
            stackTop -= argCount + 1;
            push(result);
        } catch (const RuntimeError& e) {
            throw; // Propagate RuntimeError so it can be caught by EZ or VM loop
        } catch (const std::exception& e) {
            runtimeError(std::string("Native function error: ") + e.what());
            return false;
        }
        return true;
    }

    if (callee.isBoundMethod()) {
        auto bound = callee.asBoundMethod();
        
        // We need to insert the receiver as the first argument (slot 0),
        // and replace the callee placeholder with the separated method.
        // Current stack: [..., BoundMethod, arg0, arg1, ..., argN-1]
        // Required stack: [..., method,      receiver, arg0, arg1, ..., argN-1]
        
        // Grow the stack by 1 to make room for the extra parameter
        push(Value()); 
        
        Value* calleePtr = stackTop - argCount - 2;
        
        // Shift existing arguments to the right
        for (Value* p = stackTop - 1; p > calleePtr + 1; --p) {
            *p = *(p - 1);
        }
        
        // Set method as the new callee, and the receiver as the first argument
        *calleePtr = bound->method;
        *(calleePtr + 1) = bound->receiver;
        
        // Dispatch the pure method with +1 argument count
        bool result = dispatchCall(bound->method, argCount + 1);
        // After recursive dispatch, sync our local-to-run stackTop
        this->stackTop = stackTop; 
        return result;
    }
    
    if (callee.isClosure() || callee.isFunction()) {
        BytecodeFunctionPtr bcFunc;
        std::shared_ptr<EZClosure> closure;

        if (callee.isClosure()) {
            closure = callee.asClosure();
            bcFunc = closure->function;
        } else {
            auto ezFunc = callee.asFunction();
            auto it = compiledFunctionCache.find(ezFunc.get());
            if (it != compiledFunctionCache.end()) {
                bcFunc = it->second;
            } else {
                bcFunc = compileEZFunction(ezFunc.get());
            }
        }

        if (!bcFunc) {
            runtimeError("Function compilation failed");
            return false;
        }

        // --- Arity and Variadic handling ---
        if (bcFunc->isVariadic) {
            size_t restIndex = bcFunc->arity > 0 ? bcFunc->arity - 1 : 0;
            auto restArray = std::make_shared<EZArray>();
            if (argCount >= bcFunc->arity) {
                for (size_t i = restIndex; i < argCount; ++i) {
                    restArray->push_back(*(this->stackTop - argCount + i));
                }
                this->stackTop -= (argCount - restIndex);
                argCount = restIndex;
            }
            while (argCount < restIndex) {
                *stackTop++ = Value();
                argCount++;
            }
            *stackTop++ = Value(restArray);
            argCount++;
        } else {
            size_t minArity = (bcFunc->arity > bcFunc->defaultParamCount)
                               ? bcFunc->arity - bcFunc->defaultParamCount : 0;
            
            size_t reportedMinArity = bcFunc->isMethod && minArity > 0 ? minArity - 1 : minArity;
            size_t reportedArgCount = bcFunc->isMethod && argCount > 0 ? argCount - 1 : argCount;
            size_t reportedArity = bcFunc->isMethod && bcFunc->arity > 0 ? bcFunc->arity - 1 : bcFunc->arity;
            
            if (argCount < minArity) {
                runtimeError("'" + bcFunc->name + "' expected at least " + std::to_string(reportedMinArity) + " args but got " + std::to_string(reportedArgCount));
                return false;
            }
            if (argCount > bcFunc->arity) {
                runtimeError("'" + bcFunc->name + "' expected at most " + std::to_string(reportedArity) + " args but got " + std::to_string(reportedArgCount));
                return false;
            }
        }
        
        ClosureState cs;
        if (closure) cs.upvalues = closure->upvalues;
        
        if (bcFunc->isAsync && !bypassAsyncCheck) {
            auto ezFut = std::make_shared<EZFuture>();
            
            // Extract arguments
            std::vector<Value> closedArgs;
            for (int i = 0; i < argCount; ++i) {
                closedArgs.push_back(*(this->stackTop - argCount + i));
            }
            
            // Snapshot VM state needed
            auto globalEnv = this->globalEnv;
            auto slotNames = this->globalSlotNames;
            auto slotValues = this->globalSlots;
            Value closedFunc = callee;
            bool shouldTrace = this->traceExecution;
            
            EventLoop::instance().pushTask([ezFut, globalEnv, slotNames, slotValues, closedFunc, closedArgs, shouldTrace]() {
                try {
                    auto taskVM = std::make_shared<BytecodeVM>(globalEnv);
                    taskVM->setGlobalSlots(slotNames, slotValues);
                    taskVM->taskFuture = ezFut;
                    taskVM->traceExecution = shouldTrace;
                    taskVM->isAsyncTask = true;
                    taskVM->push(closedFunc);
                    for (auto& a : closedArgs) taskVM->push(a);
                    
                    if (taskVM->dispatchCall(closedFunc, closedArgs.size(), true)) {
                        taskVM->isYielded = false;
                        taskVM->run(0); // run until completion or yield
                        
                        if (!taskVM->isYielded) {
                            if (!taskVM->running && !taskVM->frames.empty()) {
                                std::string errMsg = taskVM->pendingException.isString() ? taskVM->pendingException.toString() : "Async task failed with an exception";
                                ezFut->setError(errMsg);
                            } else {
                                Value result = (taskVM->stackTop > taskVM->stack.data()) ? *(taskVM->stackTop - 1) : Value();
                                ezFut->set(result);
                            }
                        }
                        // If yielded, OP_AWAIT inside taskVM handles setting up resumption.
                    } else {
                        // Native async function? (rare)
                        if (!taskVM->running) {
                            ezFut->setError("Async native function failed");
                        } else {
                            Value result = (taskVM->stackTop > taskVM->stack.data()) ? *(taskVM->stackTop - 1) : Value();
                            ezFut->set(result);
                        }
                    }
                } catch (const std::exception& e) {
                    ezFut->setError(e.what());
                }
            });
            
            this->stackTop -= (argCount + 1);
            push(Value::makeFuture(ezFut));
            return false; // Result is already pushed (Future)
        }

        // SYNC before pushing frame so the frame starts at the correct stack boundary
        this->stackTop = stackTop;
        pushCallFrame(bcFunc, argCount, cs);
        return true;
    }

    if (callee.isSuper()) {
        auto super_val = callee.asSuper();
        Value initMethod = super_val->parentKlass->methods.count("init")
                         ? super_val->parentKlass->methods.at("init")
                         : Value();
        
        if (initMethod.isNil()) {
            runtimeError("Parent has no 'init' method for super(...) call");
            return false;
        }

        // Use BoundMethod to handles self-injection correctly
        Value bound = Value(std::make_shared<EZBoundMethod>(Value(super_val->instance), initMethod));
        *(stackTop - argCount - 1) = bound;
        bool result = dispatchCall(bound, argCount);
        this->stackTop = stackTop;
        return result;
    }

    if (callee.isClass()) {
        auto klass = callee.asClass();
        std::vector<Value> args;
        for (int i = 0; i < argCount; i++) {
            args.push_back(peek(argCount - 1 - i));
        }
        
        // Similar to NativeFunction, we must call instantiate before popping
        // to avoid stack overlap if the constructor is an EZ function.
        Value instance = instantiate(klass, args, 0, "constructor");
        
        stackTop -= argCount + 1; // Pop args and callee
        push(instance);
        return true;
    }

    runtimeError("Value is not callable: " + callee.typeName());
    return false;
}

void BytecodeVM::pushCallFrame(BytecodeFunctionPtr bcFunc, uint8_t argCount, ClosureState cs) {
    // Enforce maximum call depth to catch unbounded recursion cleanly
    if (frames.size() >= FRAMES_MAX) {
        runtimeError("Stack overflow: maximum call depth (" + std::to_string(FRAMES_MAX) + ") exceeded");
        return;
    }

    // Push nil for any missing optional parameters
    while (argCount < bcFunc->arity) {
        *stackTop++ = Value();
        argCount++;
    }

    // --- Create Frame ---
    CallFrame newFrame;
    newFrame.function     = bcFunc;
    newFrame.ip           = bcFunc->chunk.code.data();
    newFrame.slots        = stackTop - argCount;
    // Ensure all local slots are reserved (padding for locals beyond parameters)
    while ((stackTop - newFrame.slots) < (long long)bcFunc->localCount) {
        *stackTop++ = Value();
    }
    
    this->stackTop = stackTop; // Sync back to member!

    newFrame.functionName = bcFunc->name;
    newFrame.filename     = bcFunc->filename;  // propagate source file
    newFrame.line         = 0;
    newFrame.localCount   = bcFunc->localCount;
    
    frames.push_back(newFrame);
    frameUpvalues.push_back(std::move(cs));
}

// ============================================================================

void BytecodeVM::push(const Value& value) {
    if (stackTop >= stack.data() + STACK_MAX) {
        runtimeError("Stack overflow");
        throw std::runtime_error("Stack overflow");
    }
    *stackTop++ = value;
}

Value BytecodeVM::pop() {
    if (stackTop <= stack.data()) {
        runtimeError("Stack underflow");
        throw std::runtime_error("Stack underflow");
    }
    return *--stackTop;
}

Value& BytecodeVM::peek(int distance) {
    return stackTop[-1 - distance];
}

void BytecodeVM::popN(size_t count) {
    stackTop -= count;
}

// ============================================================================
// Upvalue Handling
// ============================================================================

UpvalueObj* BytecodeVM::captureUpvalue(Value* local) {
    // Walk the open upvalue list to find an existing capture
    UpvalueObj* prev = nullptr;
    UpvalueObj* cur  = openUpvalues;
    while (cur != nullptr && cur->location > local) {
        prev = cur;
        cur  = cur->next;
    }
    if (cur != nullptr && cur->location.load() == local) return cur;

    // Create a new open upvalue
    auto uv = std::make_unique<UpvalueObj>();
    uv->location.store(local);
    uv->next       = cur;
    UpvalueObj* raw = uv.get();
    if (prev == nullptr) openUpvalues = raw;
    else prev->next = raw;
    allUpvalues.push_back(std::move(uv));
    return raw;
}

void BytecodeVM::closeUpvalues(Value* last) {
    while (openUpvalues != nullptr && openUpvalues->location >= last) {
        UpvalueObj* uv = openUpvalues;
        uv->closed     = *uv->location.load();
        uv->location.store(&uv->closed);
        openUpvalues   = uv->next;
    }
}

// ============================================================================
// Binary Operations
// ============================================================================

void BytecodeVM::doAdd() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() + b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   + b.asFloat()));   return; }
    if (a.isString() && b.isString()) {
        auto cs = std::make_shared<EZConcatString>();
        cs->left = a;
        cs->right = b;
        cs->length = a.stringLength() + b.stringLength();
        push(Value(cs));
        return;
    }
    if (a.isString()  || b.isString())  { push(Value(a.toString()  + b.toString()));  return; }
    if (a.isArray()   && b.isArray()) {
        auto res = a.asArray();
        for (const Value& v : b.asArray()) res.push_back(v);
        push(Value::makeArrayCopy(res));
        return;
    }
    runtimeError("'+' operands must be numbers, strings, or arrays");
}

void BytecodeVM::doSubtract() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() - b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   - b.asFloat()));   return; }
    runtimeError("'-' operands must be numbers");
}

void BytecodeVM::doMultiply() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() * b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   * b.asFloat()));   return; }
    // "str" * N  repetition
    if (a.isString() && b.isInteger()) {
        std::string result;
        for (long long i = 0; i < b.asInteger(); i++) result += a.asString();
        push(Value(result));
        return;
    }
    runtimeError("'*' operands must be numbers");
}

void BytecodeVM::doDivide() {
    Value b = pop(), a = pop();
    if (b.isInteger() && b.asInteger() == 0) { runtimeError("Division by zero"); return; }
    if (b.isFloat()   && b.asFloat()   == 0) { runtimeError("Division by zero"); return; }
    if (a.isInteger() && b.isInteger()) {
        long long ai = a.asInteger();
        long long bi = b.asInteger();
        if (ai % bi == 0) {
            push(Value(ai / bi));
        } else {
            push(Value(static_cast<double>(ai) / static_cast<double>(bi)));
        }
        return;
    }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   / b.asFloat()));   return; }
    runtimeError("'/' operands must be numbers");
}

void BytecodeVM::doModulo() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) {
        if (b.asInteger() == 0) { runtimeError("Modulo by zero"); return; }
        push(Value(a.asInteger() % b.asInteger()));
        return;
    }
    if (a.isNumber() && b.isNumber()) {
        push(Value(std::fmod(a.asFloat(), b.asFloat())));
        return;
    }
    runtimeError("'%' operands must be numbers");
}

void BytecodeVM::doPower() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(std::pow(a.asFloat(), b.asFloat())));
        return;
    }
    runtimeError("'**' operands must be numbers");
}

void BytecodeVM::doNegate() {
    Value a = pop();
    if (a.isInteger()) { push(Value(-a.asInteger())); return; }
    if (a.isFloat())   { push(Value(-a.asFloat()));   return; }
    runtimeError("Negation operand must be a number");
}

void BytecodeVM::doBitwiseAnd() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) { push(Value(a.asInteger() & b.asInteger())); return; }
    runtimeError("'&' operands must be numbers");
}
void BytecodeVM::doBitwiseOr() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) { push(Value(a.asInteger() | b.asInteger())); return; }
    runtimeError("'|' operands must be numbers");
}
void BytecodeVM::doBitwiseXor() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) { push(Value(a.asInteger() ^ b.asInteger())); return; }
    runtimeError("'^' operands must be numbers");
}
void BytecodeVM::doBitwiseNot() {
    Value a = pop();
    if (a.isNumber()) { push(Value(~a.asInteger())); return; }
    runtimeError("'~' operand must be a number");
}
void BytecodeVM::doShiftLeft() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asInteger() << b.asInteger())); return;
    }
    runtimeError("'<<' operands must be numbers");
}
void BytecodeVM::doShiftRight() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) {
        push(Value(a.asInteger() >> b.asInteger())); return;
    }
    runtimeError("'>>' operands must be numbers");
}

void BytecodeVM::doIndexGet() {
    Value idx = pop();
    Value obj = pop();
    if (obj.isArray()) {
        auto& arr = obj.asArray();
        long long i = idx.asInteger();
        if (i < 0 || i >= (long long)arr.size()) { runtimeError("Array index out of bounds"); return; }
        push(arr[i]);
    } else if (obj.isTuple()) {
        auto& tup = obj.asTuple();
        long long i = idx.asInteger();
        if (i < 0 || i >= (long long)tup.size()) { runtimeError("Tuple index out of bounds"); return; }
        push(tup[i]);
    } else if (obj.isString()) {
        const std::string& s = obj.asString();
        long long i = idx.asInteger();
        if (i < 0 || i >= (long long)s.length()) { runtimeError("String index out of bounds"); return; }
        push(Value(std::string(1, s[i])));
    } else if (obj.isDictionary()) {
        auto dictPtr = obj.asDictionaryPtr();
        std::shared_lock<std::shared_mutex> lk(dictPtr->map_mutex);
        std::string sKey = idx.toString();
        auto it = dictPtr->map.find(sKey);
        push(it != dictPtr->map.end() ? it->second : Value());
    } else if (obj.isBuffer()) {
        auto& buf = obj.asBuffer();
        long long i = idx.asInteger();
        if (i < 0 || i >= (long long)buf.size()) { runtimeError("Buffer index out of bounds"); return; }
        push(Value((long long)buf[i]));
    } else {
        runtimeError("Cannot index " + obj.typeName());
    }
}

void BytecodeVM::doIndexSet() {
    Value val = pop();
    Value idx = pop();
    Value obj = pop();
    if (obj.isArray()) {
        auto& arr = obj.asArray();
        long long i = idx.asInteger();
        if (i < 0) { runtimeError("Array index out of bounds"); return; }
        if (i >= (long long)arr.size()) arr.resize(i + 1);
        arr[i] = val;
    } else if (obj.isDictionary()) {
        auto dictPtr = obj.asDictionaryPtr();
        std::unique_lock<std::shared_mutex> lk(dictPtr->map_mutex);
        dictPtr->map[idx.toString()] = val;
    } else if (obj.isBuffer()) {
        auto& buf = obj.asBuffer();
        long long i = idx.asInteger();
        if (i < 0 || i >= (long long)buf.size()) { runtimeError("Buffer index out of bounds"); return; }
        buf[i] = (uint8_t)val.asInteger();
    } else {
        runtimeError("Cannot set index on " + obj.typeName());
    }
    push(val);
}

// ============================================================================
// Comparisons
// ============================================================================

void BytecodeVM::doEqual()    { Value b=pop(),a=pop(); push(Value(a.equals(b))); }
void BytecodeVM::doNotEqual() { Value b=pop(),a=pop(); push(Value(!a.equals(b))); }

void BytecodeVM::doLess() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() < b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   < b.asFloat()));   return; }
    if (a.isString()  && b.isString())  { push(Value(a.asString()  < b.asString()));  return; }
    runtimeError("'<' operands must be numbers or strings");
}
void BytecodeVM::doLessEq() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() <= b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   <= b.asFloat()));   return; }
    if (a.isString()  && b.isString())  { push(Value(a.asString()  <= b.asString()));  return; }
    runtimeError("'<=' operands must be numbers or strings");
}
void BytecodeVM::doGreater() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() > b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   > b.asFloat()));   return; }
    if (a.isString()  && b.isString())  { push(Value(a.asString()  > b.asString()));  return; }
    runtimeError("'>' operands must be numbers or strings");
}
void BytecodeVM::doGreaterEq() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(a.asInteger() >= b.asInteger())); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   >= b.asFloat()));   return; }
    if (a.isString()  && b.isString())  { push(Value(a.asString()  >= b.asString()));  return; }
    runtimeError("'>=' operands must be numbers or strings");
}
void BytecodeVM::doNot() { push(Value(!pop().isTruthy())); }

// ============================================================================
// Error Handling
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
        // Ã¢â€â‚¬Ã¢â€â‚¬ Print the error header Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬Ã¢â€â‚¬
        // Use ANSI escapes if the terminal supports them (Windows 10+)
        HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
        DWORD  consoleMode = 0;
        bool   ansi = GetConsoleMode(hErr, &consoleMode) != 0;
        if (ansi) SetConsoleMode(hErr, consoleMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);

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
    throw RuntimeError(message, faultLine);
}

void BytecodeVM::throwException(const std::string& className, const std::string& message, int line, const std::string& filename) {
    Value classVal = globalEnv->get(className);
    if (classVal.isClass()) {
        auto inst = std::make_shared<EZInstance>(classVal.asClass());
        inst->properties["message"] = Value(message);
        
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


void BytecodeVM::defineGlobal(const std::string& name, const Value& value) {
    globalEnv->define(name, value);
}

Value BytecodeVM::eval(const std::string& code, const std::string& filename) {
    // Sync all slot values back into globalEnv so the new code can see
    // globals that were written via STORE_GLOBAL_SLOT since the last sync.
    for (size_t i = 0; i < globalSlots.size() && i < globalSlotNames.size(); ++i) {
        if (!globalSlotNames[i].empty()) {
            globalEnv->assign(globalSlotNames[i], globalSlots[i]);
        }
    }

    Lexer lexer(code, filename);
    std::vector<Token> tokens = lexer.tokenize();
    if (lexer.hasError()) return Value();

    Parser parser(tokens);
    std::vector<StmtPtr> statements = parser.parse();
    if (parser.hasError()) return Value();

    BytecodeCompiler compiler;
    CompileResult result = compiler.compile(statements);
    if (!result.success) return Value();

    // Merge any new slots the eval'd code introduced
    if (!result.globalSlotNames.empty()) {
        size_t newCount = result.globalSlotNames.size();
        if (newCount > globalSlots.size()) {
            globalSlots.resize(newCount, Value());
            globalSlotNames.resize(newCount);
        }
        for (size_t i = 0; i < newCount; ++i) {
            if (globalSlotNames[i].empty() && !result.globalSlotNames[i].empty()) {
                globalSlotNames[i] = result.globalSlotNames[i];
                // Seed from globalEnv if already defined
                if (globalEnv->contains(globalSlotNames[i]))
                    globalSlots[i] = globalEnv->get(globalSlotNames[i]);
            }
        }
    }

    return execute(result.mainFunction);
}

std::string BytecodeVM::stringify(const Value& val, int line, const std::string& filename) {
    return val.toString();
}

// ============================================================================
// Built-ins
// ============================================================================

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
        if (inst->auditLog) {
            for (const auto& e : *inst->auditLog) {
                auto d = std::make_shared<EZDictionary>();
                d->map["field"]     = Value(e.field);
                d->map["old"]       = e.oldValue;
                d->map["new"]       = e.newValue;
                d->map["via"]       = Value(e.via);
                d->map["timestamp"] = Value(e.timestamp);
                result->elements.push_back(Value(d));
            }
        }
        return Value(result);
    }));

    // audit_clear(obj) Ã¢â€ â€™ clears audit log
    defineGlobal("audit_clear", Value::makeNativeFunction("audit_clear", 1, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (args[0].isInstance() && args[0].asInstance()->auditLog)
            args[0].asInstance()->auditLog->clear();
        return Value();
    }));

    // audit_since(obj, timestamp) Ã¢â€ â€™ list of entries since timestamp
    defineGlobal("audit_since", Value::makeNativeFunction("audit_since", 2, [](RuntimeContext& ctx, std::vector<Value> args) -> Value {
        if (!args[0].isInstance()) { ctx.runtimeError("audit_since() expects a model instance"); return Value(); }
        auto inst = args[0].asInstance();
        long long since = args[1].isInteger() ? args[1].asInteger() : (long long)args[1].asFloat();
        auto result = std::make_shared<EZArray>();
        if (inst->auditLog) {
            for (const auto& e : *inst->auditLog) {
                if (e.timestamp < since) continue;
                auto d = std::make_shared<EZDictionary>();
                d->map["field"]     = Value(e.field);
                d->map["old"]       = e.oldValue;
                d->map["new"]       = e.newValue;
                d->map["via"]       = Value(e.via);
                d->map["timestamp"] = Value(e.timestamp);
                result->elements.push_back(Value(d));
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
            std::shared_lock<std::shared_mutex> lk(inst->prop_mutex);
            for (const auto& [k, v] : inst->properties) snap->map[k] = v;
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
        for (const auto& [k, v] : snap->map) inst->setProperty(k, v);
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
        for (const auto& [k, vb] : b->map) {
            auto it = a->map.find(k);
            Value va = (it != a->map.end()) ? it->second : Value();
            if (va.toString() != vb.toString()) {
                auto entry = std::make_shared<EZDictionary>();
                entry->map["was"] = va;
                entry->map["now"] = vb;
                result->map[k] = Value(entry);
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
    auto it = compiledFunctionCache.find(func);
    if (it != compiledFunctionCache.end()) return it->second;

    BytecodeCompiler compiler;

    // Build a minimal TaskStmt from the EZFunction
    TaskStmt fakeTask(func->name, func->params, std::vector<TypeASTPtr>(func->params.size(), std::make_shared<TypeAST>("Any")), func->defaultValues, nullptr,
                      func->body, func->isVariadic);
    BytecodeFunctionPtr bfunc = compiler.compileFunction(fakeTask, func->name);

    compiledFunctionCache[func] = bfunc;
    
    // Clear AST data to free memory, as it's no longer needed after bytecode compilation
    func->body.clear();
    func->defaultValues.clear();
    
    return bfunc;
}

// ============================================================================
// callFunction (from native / external code)
// ============================================================================

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
    bool savedRunning = running;
    running = true;

    push(callee);
    for (const auto& arg : args) {
        push(arg);
    }

    Value result;
    if (dispatchCall(callee, args.size())) {
        run(frames.size());
        if (stackTop > stack.data() + stackBefore) {
            result = *(stackTop - 1);
        }
    }
    
    // Always restore stack to exactly where it was before the call
    stackTop = stack.data() + stackBefore;

    running = savedRunning;
    return result;
}

Value BytecodeVM::instantiate(std::shared_ptr<EZClass> klass,
                               const std::vector<Value>& args,
                               int line,
                               const std::string& filename) {
    auto inst = std::make_shared<EZInstance>(klass);
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
