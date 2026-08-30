#include "runtime/objects/EZObjects.h"
#include "gc/CycleCollector.h"
#include "sqlite3.h"
#include "eventloop/EventLoop.h"
#include "vm/BytecodeVM.h"
#include "runtime/Value.h"
#include "runtime/EZFuture.h"
#include <iostream>
#include <cmath>
#include <limits>
#include <chrono>

// EZ's wrapping integer semantics live in one shared header so that the VM and
// the compiler's constant folder cannot drift apart (see utils/WrapArith.h).
#include "utils/WrapArith.h"
#include <algorithm>
#include <cctype>
#include <shared_mutex>

// ── Suggestions for a name that was not found ────────────────────────────────
//
// "Property 'lenght' does not exist" is correct and the reader still has to go
// and look at the model to see what it should have been. A misspelling is by
// far the most likely cause of a name miss, so the candidates are worth
// searching: the fix is usually two characters away.
namespace {

// Edit distance, abandoned as soon as it exceeds `limit`.
size_t ezNameDistance(const std::string& a, const std::string& b, size_t limit) {
    if (a == b) return 0;
    if (a.size() > b.size() + limit || b.size() > a.size() + limit) return limit + 1;

    std::vector<size_t> previous(b.size() + 1), current(b.size() + 1);
    for (size_t j = 0; j <= b.size(); ++j) previous[j] = j;
    for (size_t i = 1; i <= a.size(); ++i) {
        current[0] = i;
        size_t best = current[0];
        for (size_t j = 1; j <= b.size(); ++j) {
            size_t cost = (std::tolower((unsigned char)a[i - 1]) ==
                           std::tolower((unsigned char)b[j - 1])) ? 0 : 1;
            current[j] = std::min({ previous[j] + 1, current[j - 1] + 1, previous[j - 1] + cost });
            best = std::min(best, current[j]);
        }
        if (best > limit) return limit + 1;
        previous = current;
    }
    return previous[b.size()];
}

// "  Did you mean 'name'?" for the closest candidates, or "" if none is close
// enough to be worth guessing at.
std::string ezDidYouMean(const std::string& wanted, const std::vector<std::string>& candidates) {
    if (wanted.empty() || candidates.empty()) return "";
    // One edit for a short name: at two edits, a three-letter name matches
    // most other three-letter names and the suggestion is noise.
    size_t limit = wanted.size() <= 4 ? 1 : 2;

    std::vector<std::pair<size_t, std::string>> scored;
    for (const std::string& candidate : candidates) {
        if (candidate == wanted || candidate.empty()) continue;
        if (candidate.rfind("__", 0) == 0) continue;      // internals are not suggestions
        size_t d = ezNameDistance(wanted, candidate, limit);
        if (d <= limit) scored.push_back({ d, candidate });
    }
    if (scored.empty()) return "";
    std::sort(scored.begin(), scored.end());

    std::string out = "\n  Did you mean ";
    size_t shown = std::min<size_t>(scored.size(), 3);
    for (size_t i = 0; i < shown; ++i) {
        if (i) out += (i + 1 == shown) ? " or " : ", ";
        out += "'" + scored[i].second + "'";
    }
    return out + "?";
}

// What an instance actually has: its own fields plus every method it inherits.
std::vector<std::string> ezInstanceNames(const std::shared_ptr<EZInstance>& inst) {
    std::vector<std::string> names;
    if (!inst) return names;
    if (inst->shape) {
        std::shared_lock<std::shared_mutex> lk(inst->shape->transition_mutex);
        for (const auto& entry : inst->shape->propertyOffsets) names.push_back(entry.first);
    }
    for (auto klass = inst->klass; klass; klass = klass->parent) {
        for (const auto& entry : klass->methods) names.push_back(entry.first);
    }
    return names;
}

// Dictionary keys, capped: listing 400 keys buries the message.
//
// readMap rather than getMapCopy -- the copy would duplicate every Value in the
// dictionary just to read its keys, on an error path where the dictionary may
// be large.
std::vector<std::string> ezDictNames(const std::shared_ptr<EZDictionary>& dict) {
    std::vector<std::string> names;
    if (!dict) return names;
    dict->readMap([&names](const std::unordered_map<std::string, Value>& entries) {
        for (const auto& entry : entries) {
            names.push_back(entry.first);
            if (names.size() >= 256) break;
        }
    });
    return names;
}

} // namespace
using ezarith::wrapAdd;
using ezarith::wrapSub;
using ezarith::wrapMul;
using ezarith::wrapNeg;

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wjump-misses-init"
#endif

void BytecodeVM::run(size_t targetFrameCount) {
    if (frames.empty()) return;

    // Marks the dispatch as live for the whole of this call, including the
    // nested run() invocations that callFunction makes. While it is live the VM
    // records faults instead of throwing them -- see dispatchDepth_ in
    // BytecodeVM.h for why a C++ exception cannot be allowed to unwind here.
    DispatchScope dispatchScope(this);
    // targetFrameCount is the frame depth to run down to: RETURN leaves run()
    // once the frame at that depth has returned.
    //
    // 0 means "run every frame to completion" -- what the async resume paths
    // (OP_AWAIT's continuation, async task VMs) ask for. It previously fell back
    // to frames.size(), i.e. the depth *at the moment of resumption*, so
    // resuming a yielded `await` inside a function stopped the instant that
    // function returned and silently abandoned all of its callers: the awaiting
    // function's own code ran, then the program just ended with no error and
    // exit 0. Only a top-level `await` worked.
    size_t startingFrameCount = (targetFrameCount > 0) ? targetFrameCount : 1;
    CallFrame* frame = &frames.back();
    const uint8_t* ip = frame->ip;
    Value* stackTop = this->stackTop;

    // Does the innermost try block belong to a frame THIS run() invocation owns?
    //
    // run(N) owns frames [N-1 .. back]. A TryBlock below that was registered by a
    // CALLER's frame, and this run must not jump into it: re-entrant calls
    // (constructors via instantiate(), FFI callbacks, sort comparators) push
    // their frames onto the same vector and then call run() again, but the
    // caller's tryStack is still visible. Unwinding into it from here resumed the
    // caller's bytecode inside the NESTED run, and callFunction() then rewound
    // the stack underneath it -- leaving the outer run to carry on with a
    // clobbered stack (a stale value sitting where a callee should be).
    //
    // When the handler is not ours, the exception is rethrown so it lands on the
    // dispatch level that owns the handler.
    auto ownsTryBlock = [&]() -> bool {
        return !tryStack.empty() && tryStack.back().frameIdx + 1 >= startingFrameCount;
    };

    // The human-readable text of a thrown value -- see BytecodeVM::describeException
    // for what it prefers and why (this local alias just keeps every call
    // site below unchanged).
    auto exceptionMessage = [this](const Value& exc) -> std::string {
        return describeException(exc);
    };

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

#if defined(__GNUC__) && !defined(__clang__)
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
        &&handle_TRY_END, &&handle_THROW, &&handle_FINALLY_START, &&handle_FINALLY_END, &&handle_TO_STRING, &&handle_PRINT, &&handle_CLOCK,
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
        // Must stay in the same order as the OpCode enum: this table is indexed
        // by the raw opcode byte, so an entry out of position dispatches the
        // wrong handler.
        &&handle_LOAD_LOCAL_W,
        &&handle_STORE_LOCAL_W,
        &&handle_MEMBER_IN,
        &&handle_ADD_LOCAL_LOCAL,
        &&handle_SUB_LOCAL_LOCAL,
        &&handle_INC_LOCAL_BY,
        &&handle_ADD_GLOBAL_LOCAL,
        &&handle_PRINT_STR,
        &&handle_INVOKE_METHOD,
        &&handle_END
    };
    // Tracing is off in every normal run, so mark the check cold: the compiler
    // keeps the cerr call out of line instead of inlining an I/O path into the
    // hottest loop in the interpreter. (--trace still works.)
    // A helper (doIndexGet, doAdd, ...) that faulted set running=false via
    // runtimeError and caught the C++ exception in its own simple frame, rather
    // than letting it unwind through this computed-goto dispatch -- whose goto*-
    // based control flow corrupts run()'s exception landing-pad tables, so a throw
    // from deep in the dispatch skipped run()'s own catch and crashed the process
    // (reliably, under the libuv event loop that drives every ezweb handler).
    // Route the fault to the unwinder here, which is a plain goto -- no C++
    // exception crosses the dispatch. running is set false ONLY on a fault, so
    // this is a correct signal.
    #define DISPATCH() { \
        if (__builtin_expect(!running, 0)) goto handle_vm_fault; \
        if (__builtin_expect(traceExecution, 0)) std::cerr << "[VM-TRACE] OP: " << (int)(*ip) << " at IP: " << (void*)ip << std::endl; \
        goto *dispatchTable[READ_BYTE()]; \
    }
    #define INTERPRET_LOOP DISPATCH();
    #define CASE_CODE(name) handle_##name:
#else
    #define DISPATCH() { \
        if (__builtin_expect(!running, 0)) goto handle_vm_fault; \
        break; \
    }
    #define INTERPRET_LOOP while (running && !frames.empty() && frames.size() >= startingFrameCount) { \
        if (traceExecution) std::cerr << "[VM-TRACE] OP: " << (int)(*ip) << " at IP: " << (void*)ip << std::endl; \
        uint8_t instruction = READ_BYTE(); \
        switch (static_cast<OpCode>(instruction))
    #define CASE_CODE(name) case OpCode::name:
#endif

// Deliver a fault that runtimeError() or throwException() has just recorded to
// whichever EZ `catch` owns it.
//
// EVERY fault site inside run() must end with this rather than `return`.
// Returning leaves run() on the spot, so handle_vm_fault never runs and the
// enclosing `try` never sees the error. Worse, it died *silently*: runtimeError
// only prints a report when there is no handler at all, so a program with a try
// block around the failing line exited 70 with no message whatsoever --
//
//     n = someDict["missing"]        # nil
//     try { give n.field } catch (e) { give "caught" }   # never reached
//
// The label is function-scoped, so this works under both the computed-goto and
// the switch dispatch.
#define RAISE_FAULT() goto handle_vm_fault

    dispatch_start:
    try {
        INTERPRET_LOOP {
                CASE_CODE(LOAD_CONST) {
                    uint16_t idx = READ_SHORT();
                    // Guard against a corrupt/hostile bytecode constant index
                    // (e.g. from a tampered .ezc bundle). Predictable branch, so
                    // effectively free for valid bytecode.
                    if (__builtin_expect(idx >= frame->function->chunk.resolvedConstants.size(), 0)) {
                        SYNC_IP();
                        runtimeError("Invalid bytecode: constant index out of range");
                        RAISE_FAULT();
                    }
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

                // Wide forms, for functions with more than 256 locals. The
                // compiler emits these only when the slot will not fit in a
                // byte, so the narrow cases above stay the hot path.
                CASE_CODE(LOAD_LOCAL_W) {
                    uint16_t slot = READ_SHORT();
                    *stackTop++ = frame->slots[slot];
                    DISPATCH();
                }
                CASE_CODE(STORE_LOCAL_W) {
                    uint16_t slot = READ_SHORT();
                    frame->slots[slot] = *(stackTop - 1);
                    DISPATCH();
                }

                // needle in haystack -> bool
                //
                // Reads the way the rest of the language does -- `when not (k in
                // b)` rather than `when not has_key(b, k)`. What it means
                // depends on the container, matching how each is normally asked
                // about:
                //   array/tuple : does any ELEMENT equal the needle
                //   dictionary  : is the needle a KEY (the useful question; the
                //                 values are reachable, the keys are the index)
                //   string      : is the needle a SUBSTRING
                CASE_CODE(MEMBER_IN) {
                    {
                        Value haystack = *(--stackTop);
                        Value needle   = *(--stackTop);
                        bool found = false;

                        if (haystack.isArray()) {
                            const auto& arr = haystack.asArray();
                            for (size_t i = 0; i < arr.size(); ++i) {
                                if (arr[i].equals(needle)) { found = true; break; }
                            }
                        } else if (haystack.isTuple()) {
                            const auto& tup = haystack.asTuple();
                            for (size_t i = 0; i < tup.size(); ++i) {
                                if (tup[i].equals(needle)) { found = true; break; }
                            }
                        } else if (haystack.isDictionary()) {
                            // Any value can be written as a key, and keys are
                            // stored as strings, so compare on the same
                            // stringification the subscript uses.
                            found = haystack.asDictionaryPtr()->has(needle.toString());
                        } else if (haystack.isString()) {
                            if (!needle.isString()) {
                                SYNC_IP();
                                throwException("TypeError",
                                    "'in' on a string expects a string on the left, got " +
                                    needle.typeName());
                                RAISE_FAULT();
                            }
                            found = haystack.asString().find(needle.asString()) != std::string::npos;
                        } else {
                            SYNC_IP();
                            throwException("TypeError",
                                "'in' expects an array, tuple, dictionary or string on the "
                                "right, got " + haystack.typeName());
                            RAISE_FAULT();
                        }
                        *stackTop++ = Value(found);
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_UPVALUE) {
                    uint8_t slot = READ_BYTE();
                    const ClosureState& cs = frameUpvalues.back();
                    if (__builtin_expect(slot < cs.upvalues.size() && cs.upvalues[slot] != nullptr, 1)) {
                        Value* loc = cs.upvalues[slot]->location.load();
                        if (loc) {
                            *stackTop++ = *loc;
                        } else {
                            *stackTop++ = cs.upvalues[slot]->closed;
                        }
                    } else {
                        *stackTop++ = Value();
                    }
                    DISPATCH();
                }
                CASE_CODE(STORE_UPVALUE) {
                    uint8_t slot = READ_BYTE();
                    ClosureState& cs = frameUpvalues.back();
                    if (__builtin_expect(slot < cs.upvalues.size() && cs.upvalues[slot] != nullptr, 1)) {
                        Value* loc = cs.upvalues[slot]->location.load();
                        if (loc) {
                            *loc = *(stackTop - 1);
                        } else {
                            cs.upvalues[slot]->closed = *(stackTop - 1);
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_GLOBAL) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value v;
                        if (!globalEnv->getIfExists(name, v)) {
                            SYNC_IP();
                            std::string suggestion = ezDidYouMean(name, globalEnv->getAllNames());
                            runtimeError("Undefined variable '" + name + "'" + suggestion);
                            RAISE_FAULT();
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
                    {
                        uint16_t nameIdx = READ_SHORT();
                        const std::string& name = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        *stackTop++ = Value(globalEnv->contains(name));
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_GLOBAL_SLOT) {
                    {
                        uint16_t slot = READ_SHORT();
                        if (__builtin_expect(slot < globalEnv->globalSlots.size(), 1)) {
                            *stackTop++ = globalEnv->globalSlots[slot];
                        } else {
                            *stackTop++ = Value();
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_GLOBAL_SLOT) {
                    {
                        uint16_t slot = READ_SHORT();
                        if (__builtin_expect(slot < globalEnv->globalSlots.size(), 1)) {
                            globalEnv->globalSlots[slot] = *(stackTop - 1);
                        }
                    }
                    DISPATCH();
                }

                // ——— Issue D: Fused loop-condition superinstructions ————————————————————
                // Replaces: LOAD_LOCAL i | LOAD_LOCAL end | LESS_EQ | JUMP_IF_FALSE exit
                // With one dispatch that reads locals directly — no stack push/pop.
                CASE_CODE(LOOP_LESS_EQ_LOCAL) {
                    uint8_t  loopSlot  = READ_BYTE();
                    uint8_t  endSlot   = READ_BYTE();
                    uint32_t exitOff   = READ_INT();
                    const Value& lv = frame->slots[loopSlot];
                    const Value& ev = frame->slots[endSlot];
                    // Integer fast path — covers 100% of repeat i=0 to N loops
                    if (__builtin_expect(lv.isInteger() && ev.isInteger(), 1)) {
                        if (lv.asInteger() > ev.asInteger()) ip += exitOff;
                    } else if (lv.isNumber() && ev.isNumber()) {
                        if (lv.asNumber() > ev.asNumber())  ip += exitOff;
                    } else {
                        // Fix 1.5: non-numeric loop variable is a programmer error, not a silent exit
                        SYNC_IP();
                        runtimeError("repeat loop variable must be numeric, got: " + lv.typeName());
                        RAISE_FAULT();
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
                        // Fix 1.5: non-numeric loop variable is a programmer error, not a silent exit
                        SYNC_IP();
                        runtimeError("repeat loop variable must be numeric, got: " + lv.typeName());
                        RAISE_FAULT();
                    }
                    DISPATCH();
                }

                CASE_CODE(LOAD_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint16_t icIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value obj = *(--stackTop);
                        if (obj.isInstance()) {
                            auto inst = obj.asInstance();
                            ICCacheEntry& ic = frame->function->chunk.icEntries[icIdx];

                            // 1. Fast path for instance properties (Shape IC)
                            if (ic.shape && ic.shape == inst->shape) {
                                *stackTop++ = inst->propertyValues[ic.offset];
                            }
                            // 2. Fast path for method bindings (Class IC)
                            else if (ic.klass && ic.klass == inst->klass.get()) {
                                *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, ic.methodValue));
                            }
                            // 3. Slow path
                            else {
                                CHECK_VISIBILITY(inst->klass, propName);
                                // A FIELD takes precedence over a method of the
                                // same name, and getProperty() resolves it that
                                // way. Decide field-vs-method up front, because a
                                // function stored in a FIELD is a plain value and
                                // must NOT bind self -- only a declared method
                                // does. Binding a field-held function passed the
                                // instance as a hidden first argument, so calling
                                // it failed with "expected at most N args but got
                                // N+1", and it was mis-cached as a method so the
                                // fast path repeated the mistake.
                                bool isField = inst->hasProperty(propName);
                                Value val = inst->getProperty(propName);
                                if (val.isNil() && !isField) {
                                    // Last chance before failing: a __getattr__
                                    // on the class. Deliberately on the IC MISS
                                    // path -- a property that exists is served
                                    // by the shape/class cache above and never
                                    // reaches here, so the hook costs nothing
                                    // until an access would have failed anyway.
                                    Value hook = findGetattrHook(obj, propName);
                                    if (hook.isCallable()) {
                                        // Same shape as an overloaded operator:
                                        // lay out [callee, arg] and let the main
                                        // loop run the frame, so the hook's
                                        // RETURN lands its result exactly where
                                        // this property read should leave it.
                                        *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, hook));
                                        *stackTop++ = Value(propName);
                                        SYNC_IP();
                                        if (!dispatchCall(stackTop[-2], 1)) return;
                                        LOAD_FRAME();
                                        DISPATCH();
                                    }
                                    SYNC_IP();
                                    throwException("AttributeError",
                                        "'" + inst->klass->name + "' has no property or method '" + propName + "'" +
                                        ezDidYouMean(propName, ezInstanceNames(inst)));
                                    RAISE_FAULT();
                                }

                                if (isField) {
                                    // Cache as a shape (property) IC and return the
                                    // value untouched, function or not.
                                    size_t offset;
                                    if (inst->shape->getOffset(propName, offset)) {
                                        ic.shape = inst->shape;
                                        ic.offset = offset;
                                    }
                                    *stackTop++ = val;
                                } else if (val.isFunction() || val.isClosure() || val.isNativeFunction()) {
                                    // A genuine method: bind self, cache as class IC.
                                    ic.klass = inst->klass.get();
                                    ic.methodValue = val;
                                    *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, val));
                                } else {
                                    *stackTop++ = val;
                                }
                            }
                        } else if (obj.isDictionary()) {
                            // Two locked O(1) lookups instead of copying the whole
                            // map to read one key (which made dict.prop O(n), with a
                            // string copy + atomic refcount bump per entry).
                            auto dictPtr = obj.asDictionaryPtr();
                            Value found = dictPtr->get(propName);
                            if (!found.isNil() || dictPtr->has(propName)) {
                                *stackTop++ = found;
                            } else {
                                SYNC_IP();
                                throwException("KeyError",
                                    "no key '" + propName + "' in this dictionary" +
                                    ezDidYouMean(propName, ezDictNames(dictPtr)) +
                                    "\n  Hint: has_key(d, \"" + propName + "\") tests for a key without raising.");
                                RAISE_FAULT();
                            }
                        } else if (obj.isClass()) {
                            auto klass = obj.asClass();
                            CHECK_VISIBILITY(klass, propName);
                            if (propName == "load" && klass->behaviors.persistent && !klass->persistPath.empty()) {
                                auto loadFn = [klass, this](RuntimeContext&, const std::vector<Value>&) -> Value {
                                    auto inst = std::make_shared<EZInstance>(klass);
                                    CycleCollector::instance().track(inst, ValueType::INSTANCE);
                                    sqlite3* db = nullptr;
                                    {
                                        std::unique_lock<std::shared_mutex> lk(globalEnv->registryMutex);
                                        auto it = globalEnv->persistDBConnections.find(klass->persistPath);
                                        if (it != globalEnv->persistDBConnections.end()) {
                                            db = static_cast<sqlite3*>(it->second);
                                        } else {
                                            if (sqlite3_open(klass->persistPath.c_str(), &db) == SQLITE_OK) {
                                                const char* create_sql = "CREATE TABLE IF NOT EXISTS EZ_Persist (prop TEXT PRIMARY KEY, val TEXT);";
                                                sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);
                                                globalEnv->persistDBConnections[klass->persistPath] = db;
                                            } else {
                                                db = nullptr;
                                            }
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
                                else if (propName == "name") *stackTop++ = Value(klass->name);
                                else {
                                    // A STATIC __getattr__(cls, name) gets the
                                    // last word, so a class can answer for names
                                    // it never declared -- e.g. exposing a column
                                    // handle as Model.column for query building,
                                    // which otherwise needs an instance.
                                    Value hook = findGetattrHook(obj, propName);
                                    if (hook.isCallable()) {
                                        // Static: no self to bind, so the class
                                        // is passed explicitly as the first
                                        // argument. Layout is [callee, cls, name].
                                        *stackTop++ = hook;
                                        *stackTop++ = obj;
                                        *stackTop++ = Value(propName);
                                        SYNC_IP();
                                        if (!dispatchCall(stackTop[-3], 2)) return;
                                        LOAD_FRAME();
                                        DISPATCH();
                                    }
                                    SYNC_IP();
                                    runtimeError("Static property or method '" + propName + "' does not exist on class '" + klass->name + "'");
                                    RAISE_FAULT();
                                }
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
                            if (method.isNil()) {
                                SYNC_IP();
                                throwException("AttributeError",
                                    "no method '" + propName + "' anywhere in the parent chain");
                                RAISE_FAULT();
                            }
                            if (method.isFunction() || method.isClosure() || method.isNativeFunction()) *stackTop++ = Value(std::make_shared<EZBoundMethod>(Value(super->instance), method));
                            else *stackTop++ = method;
                        } else if (propName == "name" && (obj.isFunction() || obj.isClosure() || obj.isNativeFunction() || obj.isClass())) {
                            if (obj.isClosure()) {
                                *stackTop++ = Value(obj.asClosure()->function->name);
                            } else if (obj.isFunction()) {
                                *stackTop++ = Value(obj.asFunction()->name);
                            } else if (obj.isNativeFunction()) {
                                *stackTop++ = Value(obj.asNativeFunction()->name);
                            } else if (obj.isClass()) {
                                *stackTop++ = Value(obj.asClass()->name);
                            }
                        } else {
                            SYNC_IP();
                            throwException("TypeError",
                                "cannot read property '" + propName + "' of " +
                                (obj.isNil() ? std::string("nil") : "a " + obj.typeName() + " value") +
                                (obj.isNil() ? "\n  Hint: the value is nil -- check what produced it, not this line."
                                             : ""));
                            RAISE_FAULT();
                        }

                    }
                    DISPATCH();
                }

                CASE_CODE(STORE_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint16_t icIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value value = *(--stackTop);
                        Value obj   = *(--stackTop);
                        // This op pops 2 and pushes 1, so the upper source slot
                        // (stackTop[1]) is left holding a stale copy that the
                        // following POP won't reach. Clear it so it doesn't keep
                        // the assigned object alive / defeat the GC.
                        stackTop[1] = Value();
                        if (obj.isInstance()) {
                            auto inst = obj.asInstance();
                            // __setattr__ has to be consulted BEFORE the IC fast
                            // path: it exists so an object can observe what
                            // changed, which is worthless if a cached write can
                            // slip past it.
                            //
                            // The flag is tested inline so a class without a hook
                            // -- almost every class -- pays one predictable
                            // branch here and never makes the call.
                            if (__builtin_expect(inst->klass && inst->klass->hasSetattrHook, 0)) {
                                Value hook = findSetattrHook(obj, propName);
                                if (hook.isCallable()) {
                                    *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, hook));
                                    *stackTop++ = Value(propName);
                                    *stackTop++ = value;
                                    SYNC_IP();
                                    if (!dispatchCall(stackTop[-3], 2)) return;
                                    LOAD_FRAME();
                                    DISPATCH();
                                }
                            }
                            ICCacheEntry& ic = frame->function->chunk.icEntries[icIdx];
                            if (ic.shape && ic.shape == inst->shape) {
                                inst->propertyValues[ic.offset] = value;
                            } else {
                                CHECK_VISIBILITY(inst->klass, propName);
                                inst->setProperty(propName, value);
                                size_t offset;
                                if (inst->shape->getOffset(propName, offset)) {
                                    ic.shape = inst->shape;
                                    ic.offset = offset;
                                }
                            }
                        }
                        else if (obj.isClass()) {
                            CHECK_VISIBILITY(obj.asClass(), propName);
                            obj.asClass()->staticMembers[propName] = value;
                        }
                        else if (obj.isDictionary()) {
                            auto dictPtr = obj.asDictionaryPtr();
                            dictPtr->modifyMap([&](auto& m) { m[propName] = value; });
                        }
                        else {
                            SYNC_IP();
                            runtimeError("Cannot set property '" + propName + "' on " + obj.typeName());
                            RAISE_FAULT();
                        }
                        *stackTop++ = value;
                    }
                    DISPATCH();
                }

                CASE_CODE(INTERCEPTED_STORE_PROPERTY) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint16_t icIdx = READ_SHORT();
                        const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                        Value value = *(--stackTop);
                        Value obj   = *(--stackTop);
                        // Clear the stale upper source slot (pop-2/push-1); see
                        // STORE_PROPERTY.
                        stackTop[1] = Value();

                        if (!obj.isInstance()) {
                            if (obj.isClass()) {
                                CHECK_VISIBILITY(obj.asClass(), propName);
                                obj.asClass()->staticMembers[propName] = value;
                            } else if (obj.isDictionary()) {
                                auto dictPtr = obj.asDictionaryPtr();
                                dictPtr->modifyMap([&](auto& m) { m[propName] = value; });
                            } else {
                                SYNC_IP();
                                runtimeError("Cannot set property '" + propName + "' on " + obj.typeName());
                                RAISE_FAULT();
                            }
                            *stackTop++ = value;
                        } else {
                            auto inst  = obj.asInstance();
                            auto klass = inst->klass;

                            // This is the opcode the compiler actually emits for
                            // `obj.prop = value`, so the hook must be here as
                            // well as in STORE_PROPERTY -- and ahead of the
                            // no-behaviours fast path below, which would
                            // otherwise bypass it for every ordinary class.
                            // Flag tested inline; see STORE_PROPERTY.
                            if (__builtin_expect(klass && klass->hasSetattrHook, 0)) {
                                Value hook = findSetattrHook(obj, propName);
                                if (hook.isCallable()) {
                                    *stackTop++ = Value(std::make_shared<EZBoundMethod>(obj, hook));
                                    *stackTop++ = Value(propName);
                                    *stackTop++ = value;
                                    SYNC_IP();
                                    if (!dispatchCall(stackTop[-3], 2)) return;
                                    LOAD_FRAME();
                                    DISPATCH();
                                }
                            }

                            if (!klass->behaviors.any()) {
                                CHECK_VISIBILITY(klass, propName);
                                ICCacheEntry& ic = frame->function->chunk.icEntries[icIdx];
                                if (ic.shape && ic.shape == inst->shape) {
                                    std::unique_lock<std::shared_mutex> lk(inst->prop_mutex);
                                    inst->propertyValues[ic.offset] = value;
                                } else {
                                    inst->setProperty(propName, value);
                                    size_t offset;
                                    if (inst->shape->getOffset(propName, offset)) {
                                        ic.shape = inst->shape;
                                        ic.offset = offset;
                                    }
                                }
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
                                            RAISE_FAULT();
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
                                    AuditEntry e;
                                    e.field     = propName;
                                    e.oldValue  = oldValue;
                                    e.newValue  = value;
                                    e.via       = frame->function->name;
                                    e.timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                                                      std::chrono::system_clock::now().time_since_epoch()).count();
                                    inst->modifyAuditLog([&](auto& log) { log.push_back(std::move(e)); });
                                }

                                if (klass->behaviors.hasCached && inst->getCacheStore()) {
                                    inst->modifyCacheStore([&](auto& cache) {
                                        for (auto& [methodName, cr] : cache) {
                                            // `cr.deps` is read here but never populated anywhere in
                                            // the codebase, so this condition was always false and a
                                            // @cached result was frozen for the life of the instance:
                                            // a cart's total() kept reporting the price it was first
                                            // called with, no matter how the price changed. Until
                                            // reads are actually tracked, invalidate every cached
                                            // result on the instance when any property changes --
                                            // conservative (it can recompute more than strictly
                                            // needed) but never stale, which is the right way round
                                            // for a cache the caller cannot see.
                                            (void)propName;
                                            cr.dirty = true;
                                        }
                                    });
                                }

                                if (klass->behaviors.persistent && !klass->persistPath.empty()) {
                                    sqlite3* db = nullptr;
                                    {
                                        std::unique_lock<std::shared_mutex> lk(globalEnv->registryMutex);
                                        auto it = globalEnv->persistDBConnections.find(klass->persistPath);
                                        if (it != globalEnv->persistDBConnections.end()) {
                                            db = static_cast<sqlite3*>(it->second);
                                        } else {
                                            if (sqlite3_open(klass->persistPath.c_str(), &db) == SQLITE_OK) {
                                                const char* create_sql = "CREATE TABLE IF NOT EXISTS EZ_Persist (prop TEXT PRIMARY KEY, val TEXT);";
                                                sqlite3_exec(db, create_sql, nullptr, nullptr, nullptr);
                                                globalEnv->persistDBConnections[klass->persistPath] = db;
                                            } else {
                                                db = nullptr;
                                            }
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
                        std::unique_lock<std::shared_mutex> lk(globalEnv->registryMutex);
                        auto& win = globalEnv->rateLimiterRegistry[key];
                        while (!win.empty() && now - win.front() > windowMs) win.pop_front();
                        if ((long long)win.size() >= maxCnt) {
                            long long waitMs = windowMs - (now - win.front());
                            lk.unlock();
                            SYNC_IP();
                            runtimeError("RateLimitError: rate limit exceeded for '" + taskName + "'. Retry in " + std::to_string(waitMs) + "ms");
                            RAISE_FAULT();
                        }
                        win.push_back(now);

                        // Fix 1.3: Prevent unbounded growth by periodically sweeping empty or stale entries
                        static int rateLimitSweepCounter = 0;
                        if (++rateLimitSweepCounter > 1000) {
                            rateLimitSweepCounter = 0;
                            for (auto it = globalEnv->rateLimiterRegistry.begin(); it != globalEnv->rateLimiterRegistry.end(); ) {
                                if (it->second.empty() || now - it->second.back() > 86400000LL) {
                                    it = globalEnv->rateLimiterRegistry.erase(it);
                                } else {
                                    ++it;
                                }
                            }
                        }
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
                            if (inst->getCacheStore()) {
                                auto it = inst->getCacheStore()->find(methodName);
                                if (it != inst->getCacheStore()->end() && !it->second.dirty) {
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
                            inst->modifyCacheStore([&](auto& cache) {
                                auto& cr = cache[methodName];
                                cr.result = result;
                                cr.dirty  = false;
                            });
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(POP)  { --stackTop; *stackTop = Value(); } DISPATCH();
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
                    if (__builtin_expect(v.isInteger(), 1)) { // INTEGER fast path
                        v = Value(v.asIntegerUnsafe() + 1LL);
                    } else if (v.isFloat()) { // NUMBER (double)
                        v = Value(v.asFloatUnsafe() + 1.0);
                    } else {
                        v = Value(v.asNumber() + 1.0);
                    }
                    DISPATCH();
                }
                CASE_CODE(DEC_LOCAL) {
                    Value& v = frame->slots[READ_BYTE()];
                    if (__builtin_expect(v.isInteger(), 1)) { // INTEGER fast path
                        v = Value(v.asIntegerUnsafe() - 1LL);
                    } else if (v.isFloat()) { // NUMBER (double)
                        v = Value(v.asFloatUnsafe() - 1.0);
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doNegate); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doNegate); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(ADD) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            // Integer ADD: keep integer semantics expected by library code
                            // (large values wrap — libs like math/collections rely on integer modulo chains).
                            // wrapAdd makes that wrap defined instead of signed-overflow UB.
                            long long res = wrapAdd(a.asInteger(), b.asInteger());
                            stackTop -= 2; *stackTop++ = Value(res);
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doAdd); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doAdd); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(SUB) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            // Integer SUB: keep integer semantics expected by library code
                            // (wrapSub makes the wrap defined instead of signed-overflow UB)
                            long long res = wrapSub(a.asInteger(), b.asInteger());
                            stackTop -= 2; *stackTop++ = Value(res);
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doSubtract); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doSubtract); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(MUL) {
                    {
                        const Value& b = stackTop[-1];
                        const Value& a = stackTop[-2];
                        if (a.isInteger() && b.isInteger()) {
                            // Integer MUL: keep integer semantics expected by library code
                            // (math lib LCG uses large multiplications followed by % to stay in range).
                            // wrapMul makes that wrap defined instead of signed-overflow UB.
                            long long res = wrapMul(a.asInteger(), b.asInteger());
                            stackTop -= 2; *stackTop++ = Value(res);
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doMultiply); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doMultiply); stackTop = this->stackTop; LOAD_FRAME();
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
                            if (bl == 0) { SYNC_IP(); throwException("ZeroDivisionError", "division by zero"); RAISE_FAULT(); }
                            
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
                            if (db == 0) { SYNC_IP(); throwException("ZeroDivisionError", "division by zero"); RAISE_FAULT(); }
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doDivide); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doDivide); stackTop = this->stackTop; LOAD_FRAME();
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
                            if (bl == 0) { SYNC_IP(); throwException("ZeroDivisionError", "modulo by zero"); RAISE_FAULT(); }
                            long long res = a.asInteger() % bl;
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doModulo); stackTop = this->stackTop; LOAD_FRAME();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(POW) { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doPower); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_AND)    { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doBitwiseAnd); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_OR)     { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doBitwiseOr); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_XOR)    { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doBitwiseXor); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(BIT_NOT)    { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doBitwiseNot); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(SHIFT_LEFT) { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doShiftLeft); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                CASE_CODE(SHIFT_RIGHT){ SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doShiftRight); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }

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
                        if (a.m_data.index() == b.m_data.index()) {
                            bool res;
                            if (a.isInteger()) res = (a.asInteger() != b.asInteger());
                            else if (a.isFloat()) res = (a.asFloat() != b.asFloat());
                            else if (a.isBool()) res = (a.asBool() != b.asBool());
                            else if (a.isNil()) res = false;
                            else { SYNC_IP(); this->stackTop = stackTop; doNotEqual(); stackTop = this->stackTop; LOAD_FRAME(); DISPATCH(); }
                            stackTop -= 2;
                            *stackTop = Value(res);
                            stackTop++;
                        } else if (a.isInstance()) {
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doLess); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doLess); stackTop = this->stackTop; LOAD_FRAME();
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doLessEq); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doLessEq); stackTop = this->stackTop; LOAD_FRAME();
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doGreater); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doGreater); stackTop = this->stackTop; LOAD_FRAME();
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
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doGreaterEq); stackTop = this->stackTop; LOAD_FRAME();
                        } else {
                            SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doGreaterEq); stackTop = this->stackTop; LOAD_FRAME();
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
                CASE_CODE(LOOP) {
                    ip -= READ_INT();
                    // Safepoint poll. Every loop and every recursive call goes
                    // through a backward jump, so a thread running EZ code is
                    // never far from one -- which is what lets the collector
                    // stop the world instead of skipping collection entirely
                    // whenever a spawn() worker is alive.
                    //
                    // The check is one relaxed atomic load; only when a
                    // collection is actually pending do we pay for flushing the
                    // frame, and that path then blocks anyway.
                    if (__builtin_expect(CycleCollector::instance().safepointPending(), 0)) {
                        SYNC_IP();      // leave the frame describing where we are
                        CycleCollector::instance().pollSafepoint();
                        LOAD_FRAME();
                    }
                    DISPATCH();
                }

                CASE_CODE(CALL) {
                    bool handled = false;
                    {
                        uint8_t argCount = READ_BYTE();
                        Value callee = *(stackTop - argCount - 1);
                        
                        if (callee.isNativeFunction() && callee.asNativeFunction()->name == "str" && argCount == 1) {
                            Value arg = *(stackTop - 1);
                            if (arg.isInstance()) {
                                auto inst = arg.asInstance();
                                Value method = inst->getProperty("toString");
                                if (method.isCallable()) {
                                    SYNC_IP();
                                    this->stackTop = stackTop;
                                    // Field-held function: call it as-is, no implicit
                                    // self. See the matching note in TO_STRING.
                                    Value bound = inst->hasProperty("toString")
                                        ? method
                                        : Value(std::make_shared<EZBoundMethod>(arg, method));
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

                            // ── __call__ magic method ──────────────────────────────────────────────
                            // If the callee is an instance that defines a __call__ method, invoke it
                            // transparently.  This enables Python-ctypes-style callable objects:
                            //   lib.add.argtypes = [c_int, c_int]
                            //   result = lib.add(10, 20)   ← lib.add is a Function instance
                            // The bound method receives `self` as its first argument (slot 0) via the
                            // normal BoundMethod path, so no special arity adjustment is needed here.
                            if (!handled && callee.isInstance()) {
                                Value callMethod = callee.asInstance()->getProperty("__call__");
                                if (callMethod.isCallable()) {
                                    // Replace the callee slot on the stack with a BoundMethod so that
                                    // dispatchCall inserts `self` automatically.
                                    Value bound = Value(std::make_shared<EZBoundMethod>(callee, callMethod));
                                    *(stackTop - argCount - 1) = bound;
                                    if (dispatchCall(bound, argCount, false, ip)) {
                                        LOAD_FRAME();
                                    } else {
                                        REFRESH_FRAME();
                                    }
                                    stackTop = this->stackTop;
                                    handled = true;
                                }
                            }

                            if (!handled) {
                                if (dispatchCall(callee, argCount, false, ip)) {
                                    LOAD_FRAME();
                                } else {
                                    REFRESH_FRAME();
                                }
                                stackTop = this->stackTop;
                            }
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
                                
                                std::vector<Value> posArgs(stackTop - posCount, stackTop);
                                stackTop -= posCount;
                                
                                for (size_t i = paramOffset; i < paramNames.size(); ++i) {
                                    size_t callerArgIdx = i - paramOffset;
                                    if (callerArgIdx < posArgs.size()) {
                                        *stackTop++ = posArgs[callerArgIdx];
                                    } else {
                                        // Was: find() on a TEMPORARY getMapCopy(), compared
                                        // against end() of a second temporary, then
                                        // dereferenced (it->second / it->first) after both
                                        // had been destroyed -- undefined behaviour, and two
                                        // O(n) map copies per parameter. Now one locked pass
                                        // that finds, extracts and erases atomically.
                                        bool  found = false;
                                        Value kwVal;
                                        dict->modifyMap([&](auto& m) {
                                            auto mit = m.find(paramNames[i]);
                                            if (mit != m.end()) {
                                                found = true;
                                                kwVal = mit->second;
                                                m.erase(mit);
                                            }
                                        });
                                        // Missing args fall through to defaults.
                                        *stackTop++ = found ? kwVal : Value();
                                    }
                                }

                                std::string unexpected;
                                dict->readMap([&](const auto& m) {
                                    if (!m.empty()) unexpected = m.begin()->first;
                                });
                                if (!unexpected.empty()) {
                                    SYNC_IP();
                                    runtimeError("Unexpected keyword argument '" + unexpected + "'");
                                    RAISE_FAULT();
                                }
                                totalArity = expectedCallerParams;
                            } else {
                                if (!kwargs.asDictionaryPtr()->empty()) {
                                    SYNC_IP();
                                    runtimeError("Unexpected keyword argument");
                                    RAISE_FAULT();
                                }
                            }
                        }
                        
                        if (!handled) {
                            SYNC_IP();
                            this->stackTop = stackTop;
                            if (dispatchCall(callee, totalArity, false, ip)) {
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

                        // Is this a tail call back into the function this frame is
                        // already running? If so we can reuse the frame instead of
                        // pushing a new one.
                        //
                        // This used to only test callee.isFunction(). But a `task`
                        // compiles to a CLOSURE (see `ez <file> --dump`), so the
                        // callee is normally an EZClosure and isFunction() is false
                        // -- the check never matched, TCO never engaged, and tail
                        // recursion piled up frames until it hit FRAMES_MAX.
                        bool isSelfTailCall = false;
                        const std::vector<std::shared_ptr<UpvalueObj>>* calleeUpvalues = nullptr;

                        if (callee.isClosure()) {
                            auto cl = callee.asClosure();
                            if (cl && cl->function == frame->function) {
                                isSelfTailCall = true;
                                calleeUpvalues = &cl->upvalues;
                            }
                        } else if (callee.isFunction()) {
                            auto ezFunc = callee.asFunction();
                            if (ezFunc) {
                                std::shared_lock<std::shared_mutex> lk(globalEnv->registryMutex);
                                auto it = globalEnv->compiledFunctionCache.find(ezFunc.get());
                                if (it != globalEnv->compiledFunctionCache.end() && it->second == frame->function) {
                                    isSelfTailCall = true;
                                }
                            }
                        }

                        if (isSelfTailCall) {
                            Value* oldTop = stackTop;
                            size_t localCount = frame->function->localCount;

                            // Any closure created during THIS iteration captured our
                            // locals by pointer. Close those upvalues before we reuse
                            // the slots, or they would silently observe the next
                            // iteration's values (or freed slots).
                            closeUpvalues(frame->slots);

                            // The callee may be a different closure instance over the
                            // same function, so adopt its upvalues for the next round.
                            if (calleeUpvalues) frameUpvalues.back().upvalues = *calleeUpvalues;

                            for (int i = 0; i < argCount; i++) {
                                frame->slots[i] = *(stackTop - argCount + i);
                            }
                            // Reset the reused frame's non-parameter locals to
                            // NIL (fresh-call semantics) so values from the prior
                            // iteration don't linger and root dead objects.
                            clearStackSlots(frame->slots + argCount, frame->slots + localCount);
                            stackTop = frame->slots + localCount;
                            // Release the abandoned callee/args/temporaries that
                            // sat above the reused frame.
                            clearStackSlots(stackTop, oldTop);
                            ip = frame->function->chunk.code.data();
                            goto tail_call_restart;
                        }

                        SYNC_IP();
                        this->stackTop = stackTop;
                        if (dispatchCall(callee, argCount, false, ip)) {
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
                        Value* oldTop = stackTop;                 // top before popping the result
                        Value result = std::move(*(--stackTop));
                        SYNC_IP();
                        Value* targetSlots = frame->slots;
                        closeUpvalues(targetSlots);

                        // Drop any try blocks owned by the frame we are leaving.
                        //
                        // `give` from inside a try block jumps straight here and
                        // never executes TRY_END, so without this the TryBlock
                        // outlives its frame. The next throw then unwound into a
                        // DEAD frame -- ip pointing into a returned function's
                        // chunk, stackTop into abandoned slots -- which did not
                        // merely crash: it swallowed the exception. A script
                        // ending in an uncaught `throw` re-entered the dead
                        // handler and exited 0, reporting success for a program
                        // that had died.
                        //
                        // frames.size()-1 is the index of the frame being popped;
                        // anything at or above it is going away with it.
                        size_t returningFrame = frames.size() - 1;
                        while (!tryStack.empty() && tryStack.back().frameIdx >= returningFrame) {
                            tryStack.pop_back();
                        }

                        frameUpvalues.pop_back();
                        frames.pop_back();

                        if (frames.empty() || frames.size() < startingFrameCount) {
                            this->stackTop = targetSlots - 1;
                            *(this->stackTop) = result;
                            this->stackTop++;
                            // Release the returning frame's abandoned slots so they
                            // don't keep dead objects alive / defeat the GC.
                            clearStackSlots(this->stackTop, oldTop);
                            return;
                        }

                        LOAD_FRAME();
                        stackTop = targetSlots - 1; // Replace callee with result
                        *stackTop++ = result;
                        // Release the returning frame's abandoned local/temporary
                        // slots [stackTop, oldTop).
                        clearStackSlots(stackTop, oldTop);
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
                        Value* oldTop = stackTop;
                        std::vector<Value> el(count);
                        for (int i = count - 1; i >= 0; i--) el[i] = *(--stackTop);
                        *stackTop++ = Value::makeArray(el);
                        // Clear the vacated element source slots (pop-N/push-1).
                        clearStackSlots(stackTop, oldTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(BUILD_TUPLE) {
                    {
                        uint8_t count = READ_BYTE();
                        Value* oldTop = stackTop;
                        std::vector<Value> el(count);
                        for (int i = count - 1; i >= 0; i--) el[i] = *(--stackTop);
                        *stackTop++ = Value::makeTuple(el);
                        clearStackSlots(stackTop, oldTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(MAKE_DICT) {
                    {
                        uint8_t pairs = READ_BYTE();
                        Value* oldTop = stackTop;
                        Value dict = Value::makeDictionary();
                        auto dictPtr = dict.asDictionaryPtr();
                        dictPtr->modifyMap([&](auto& m) {
                            m.reserve(m.size() + pairs);
                            for (int i = 0; i < pairs; i++) {
                                Value val = *(--stackTop);
                                Value key = *(--stackTop);
                                m[key.toString()] = val;
                            }
                        });
                        *stackTop++ = dict;
                        // Clear the vacated key/value source slots (pop-2N/push-1).
                        clearStackSlots(stackTop, oldTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(INDEX_GET)  { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doIndexGet); stackTop = this->stackTop; DISPATCH(); }
                CASE_CODE(INDEX_SET)  { SYNC_IP(); this->stackTop = stackTop; guardedHelper(&BytecodeVM::doIndexSet); stackTop = this->stackTop; DISPATCH(); }

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
                            auto srcCopy = src.getElementsCopy();
                            dst.modifyElements([&](auto& e) {
                                e.reserve(e.size() + srcCopy.size());
                                e.insert(e.end(), srcCopy.begin(), srcCopy.end());
                            });
                        } else if (arr.isArray() && iter.isString()) {
                            auto& dst = arr.asArray();
                            std::string s = iter.asString();
                            dst.modifyElements([&](auto& e) {
                                e.reserve(e.size() + s.size());
                                for (char c : s) {
                                    e.push_back(Value(std::string(1, c)));
                                }
                            });
                        } else {
                            SYNC_IP();
                            runtimeError("Cannot spread non-iterable value");
                            RAISE_FAULT();
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
                            RAISE_FAULT();
                        }
                        auto& args = argsArray.asArray();
                        int argc = args.size();
                        
                        // Recalculate callee after popping argsArray
                        callee = *(stackTop - 1);
                        
                        // Push all arguments to the stack
                        for (const Value& arg : args.getElementsCopy()) {
                            *stackTop++ = arg;
                        }
                        
                        // Dispatch the call normally
                        SYNC_IP();
                        if (dispatchCall(callee, argc, false, ip)) {
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
                            throwException("AttributeError",
                                "the parent model has no method '" + method + "'");
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
                        if (v.isArray() || v.isString()) {
                            *stackTop++ = Value::makeArray({v, Value(0LL)});
                        } else if (v.isDictionary()) {
                            std::vector<Value> ks;
                            {
                                auto dictPtr = v.asDictionaryPtr();
                                for (auto& [k, _] : dictPtr->getMapCopy()) ks.push_back(Value(k));
                            }
                            *stackTop++ = Value::makeArray({Value::makeArray(ks), Value(0LL)});
                        } else {
                            SYNC_IP();
                            throwException("TypeError",
                                "a " + v.typeName() + " value cannot be looped over"
                                "\n  Hint: `get x in y` needs an array, dictionary, string or range.");
                            RAISE_FAULT();
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(GET_DICT_ITER) {
                    {
                        Value v = *(--stackTop);
                        if (v.isDictionary()) {
                            std::vector<Value> items;
                            {
                                auto dictPtr = v.asDictionaryPtr();
                                for (auto& [k, val] : dictPtr->getMapCopy()) {
                                    items.push_back(Value::makeArray({Value(k), val}));
                                }
                            }
                            *stackTop++ = Value::makeArray({Value::makeArray(items), Value(0LL)});
                        } else {
                            SYNC_IP();
                            throwException("TypeError",
                                "expected a dictionary, got a " + v.typeName() + " value");
                            RAISE_FAULT();
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
                        Value dataVal = iArr[0];
                        
                        bool hasNext = false;
                        Value nextVal;
                        if (dataVal.isArray()) {
                            const auto& data = dataVal.asArray();
                            if (idx < (long long)data.size()) { hasNext = true; nextVal = data[idx]; }
                        } else if (dataVal.isString()) {
                            const auto& data = dataVal.asString();
                            if (idx < (long long)data.size()) { hasNext = true; nextVal = Value(std::string(1, data[idx])); }
                        }
                        
                        if (!hasNext) { --stackTop; ip += offset; }
                        else { 
                            iArr.set(1, Value(idx + 1)); 
                            *(stackTop - 1) = nextVal; 
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(ITER_HAS_NEXT) {
                    {
                        Value& iter = *(stackTop - 1);
                        auto& iArr = iter.asArray();
                        long long idx = iArr[1].asInteger();
                        Value dataVal = iArr[0];
                        
                        bool hasNext = false;
                        if (dataVal.isArray()) {
                            hasNext = idx < (long long)dataVal.asArray().size();
                        } else if (dataVal.isString()) {
                            hasNext = idx < (long long)dataVal.asString().size();
                        }
                        *stackTop++ = Value(hasNext);
                    }
                    DISPATCH();
                }
                
                CASE_CODE(TO_STRING) {
                    bool handled = false;
                    {
                        Value v = *(stackTop - 1);
                        if (v.isInstance()) {
                            auto inst = v.asInstance();
                            Value method = inst->getProperty("toString");
                            if (method.isCallable()) {
                                SYNC_IP();
                                this->stackTop = stackTop;
                                // A function held in a FIELD is a plain value, not a
                                // method, so calling it must not pass the instance as
                                // a hidden first argument -- same rule GET_PROPERTY
                                // applies. Binding unconditionally here meant
                                // `obj.toString = || { ... }` died with "expected at
                                // most 0 args but got 1" the moment the object was
                                // printed.
                                Value bound = inst->hasProperty("toString")
                                    ? method
                                    : Value(std::make_shared<EZBoundMethod>(v, method));
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
                    bool handled = false;
                    {
                        // Same dispatch TO_STRING uses (interpolation, string
                        // concat) -- `out obj` used to skip it and always
                        // print the bare "<instance>", so `out e` and `` `{e}` ``
                        // on the same value printed two different things.
                        Value v = *(stackTop - 1);
                        if (v.isInstance()) {
                            auto inst = v.asInstance();
                            Value method = inst->getProperty("toString");
                            if (method.isCallable()) {
                                SYNC_IP();
                                this->stackTop = stackTop;
                                Value bound = inst->hasProperty("toString")
                                    ? method
                                    : Value(std::make_shared<EZBoundMethod>(v, method));
                                *(stackTop - 1) = bound;
                                if (dispatchCall(bound, 0)) {
                                    LOAD_FRAME();
                                } else {
                                    REFRESH_FRAME();
                                }
                                stackTop = this->stackTop;
                                std::cout << (*(--stackTop)).toString() << std::endl;
                                handled = true;
                            }
                        }
                        if (!handled) {
                            std::cout << (*(--stackTop)).toString() << std::endl;
                        }
                    }
                    DISPATCH();
                }
                CASE_CODE(CLOCK) { 
                    auto now = std::chrono::system_clock::now().time_since_epoch();
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
                    *stackTop++ = Value(static_cast<long long>(ms));
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
                                if (fut->isError()) {
                                    SYNC_IP();
                                    throwException("Exception", fut->getError());
                                    RAISE_FAULT();
                                } else {
                                    *(stackTop - 1) = fut->get();
                                }
                            } else {
                                SYNC_IP();
                                this->isYielded = true;
                                this->stackTop = stackTop;
                                
                                std::shared_ptr<BytecodeVM> sharedVM = this->shared_from_this();

                                fut->then([sharedVM, fut]() {
                                    EventLoop::instance().pushTask([sharedVM, fut]() {
                                        sharedVM->isYielded = false;
                                        if (fut->isError()) {
                                            if (!sharedVM->frames.empty()) {
                                                sharedVM->frames.back().ip -= 1;
                                            }
                                        } else {
                                            *(sharedVM->stackTop - 1) = fut->get();
                                        }
                                        sharedVM->run(0);
                                        
                                        if (!sharedVM->isYielded && sharedVM->taskFuture) {
                                            if (sharedVM->isExceptionPending || !sharedVM->pendingException.isNil()) {
                                                std::string errMsg = !sharedVM->pendingException.isNil() ? sharedVM->pendingException.toString() : "Async task failed with an exception";
                                                sharedVM->taskFuture->setError(errMsg);
                                            } else {
                                                Value result = (sharedVM->stackTop > sharedVM->stack.data()) ? *(sharedVM->stackTop - 1) : Value();
                                                sharedVM->taskFuture->set(result);
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
                        CycleCollector::instance().track(klass, ValueType::CLASS);
                        
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
                                if (val.asClosure()->function->hasCached) {
                                    klass->behaviors.hasCached = true;
                                }
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
                            if (klass->parent->behaviors.hasCached) {
                                klass->behaviors.hasCached = true;
                            }
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

                        // Members and any inherited methods are in place now, so
                        // this is the point at which the attribute-hook flags can
                        // be settled. Doing it once here is what keeps the
                        // per-write cost on the property store path to one bool.
                        klass->refreshAttrHookFlags();

                        // Validate interfaces
                        for (const auto& iface : interfaces) {
                            for (const auto& methodName : iface->requiredMethods) {
                                if (klass->methods.find(methodName) == klass->methods.end()) {
                                    SYNC_IP();
                                    runtimeError("Model '" + className + "' fails to implement interface '" + 
                                                 iface->name + "': missing task '" + methodName + "'");
                                    RAISE_FAULT();
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
                            // 16-bit: the index addresses a slot in the
                            // enclosing frame, which can now exceed 255.
                            uint16_t index = READ_SHORT();
                            if (isLocal) {
                                closure->upvalues.push_back(captureUpvalue(frame->slots + index));
                            } else {
                                // index is into the enclosing closure's upvalue list
                                closure->upvalues.push_back(frameUpvalues.back().upvalues[index]);
                            }
                        }
                        // makeClosure(), not Value(closure): the raw constructor
                        // skips CycleCollector::track(), so closures built here
                        // -- which is every closure in ordinary EZ code -- were
                        // never candidates. A container holding a closure that
                        // captures it back therefore had an edge pointing
                        // outside the candidate set and could never be
                        // collected.
                        *stackTop++ = Value::makeClosure(closure);
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

                CASE_CODE(FINALLY_START) {
                    // Mark that we are inside a finally block. No-op for execution flow.
                    DISPATCH();
                }

                CASE_CODE(FINALLY_END) {
                    // If there is a pending exception saved before entering finally, re-throw it now.
                    if (!pendingException.isNil()) {
                        Value exc = pendingException;
                        pendingException = Value(); // Clear it

                        if (ownsTryBlock()) {
                            TryBlock tb = tryStack.back(); tryStack.pop_back();
                            while (frames.size() > tb.frameIdx + 1) {
                                closeUpvalues(frames.back().slots);
                                frames.pop_back(); frameUpvalues.pop_back();
                            }
                            stackTop = tb.stackTop;
                            LOAD_FRAME();
                            ip = tb.catchIp;
                            *stackTop++ = exc;
                        } else if (!tryStack.empty()) {
                            // Handler belongs to a caller's frame; hand it back
                            // via pendingException and return, rather than
                            // unwinding a C++ exception out of this dispatch (see
                            // OP_THROW for why that is unsafe).
                            pendingException = exc;
                            SYNC_IP();
                            return;
                        } else {
                            SYNC_IP();
                            runtimeError("Uncaught exception: " + exceptionMessage(exc));
                            return;
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(THROW) {
                    {
                        Value exc = *(--stackTop);
                        
                        if (exc.isDictionary()) {
                            auto dict = exc.asDictionaryPtr();
                            if (dict->has("stackTrace")) {
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
                                dict->modifyMap([&](auto& m) { m["stackTrace"] = Value(st); });
                            }
                        }
                        
                        pendingException = exc;
                        if (ownsTryBlock()) {
                            TryBlock tb = tryStack.back(); tryStack.pop_back();
                            while (frames.size() > tb.frameIdx + 1) {
                                closeUpvalues(frames.back().slots);
                                frames.pop_back(); frameUpvalues.pop_back();
                            }
                            stackTop = tb.stackTop;
                            LOAD_FRAME();
                            ip = tb.catchIp;
                            *stackTop++ = exc;
                            // The handler now owns the exception (it is on the
                            // stack as the caught variable), so it is no longer
                            // "propagating". Clear it: execute()/callFunction now
                            // read a leftover pendingException as an unhandled
                            // error, and a stale one from a CAUGHT throw made
                            // every try/catch script exit 70.
                            pendingException = Value();
                        } else if (!tryStack.empty()) {
                            // A handler exists, but it lives in a frame belonging
                            // to an OUTER run() -- we are inside a re-entrant call
                            // (a constructor, an FFI callback, a sort comparator).
                            //
                            // Do NOT jump to it (that resumes the caller's
                            // bytecode inside THIS run, and callFunction then
                            // rewinds the stack underneath it), and do NOT throw a
                            // C++ exception out of here: unwinding out of run()'s
                            // computed-goto dispatch corrupts the C++ unwinder and
                            // crashed hard under the event loop (an FFI callback
                            // whose body hit a runtime error took the process
                            // down). Hand the exception back via pendingException
                            // and RETURN; callFunction() re-raises it from
                            // normal-function context so it reaches the dispatch
                            // that owns the handler.
                            pendingException = exc;
                            SYNC_IP();
                            return;
                        } else {
                            SYNC_IP();
                            runtimeError("Uncaught exception: " + exceptionMessage(exc));
                            return;
                        }
                    }
                    DISPATCH();
                }

                // Reached (via a plain goto from DISPATCH) when a doXXX helper
                // faulted: runtimeError set pendingException + running=false and
                // the helper caught the C++ throw in its OWN simple frame, so no
                // exception unwound through this computed-goto dispatch. Route it
                // to the owning try exactly like OP_THROW -- but with a goto, not
                // a C++ throw, because a throw from here loses run()'s landing pad
                // and crashes (fatally under the libuv event loop that drives
                // every ffi callback / ezweb handler).
                handle_vm_fault: {
                    if (pendingException.isNil()) { running = true; DISPATCH(); } // defensive
                    if (ownsTryBlock()) {
                        running = true;   // resume execution in the catch handler
                        TryBlock tb = tryStack.back(); tryStack.pop_back();
                        while (frames.size() > tb.frameIdx + 1) {
                            closeUpvalues(frames.back().slots);
                            frames.pop_back(); frameUpvalues.pop_back();
                        }
                        stackTop = tb.stackTop;
                        LOAD_FRAME();
                        ip = tb.catchIp;
                        *stackTop++ = pendingException;
                        pendingException = Value();
                        DISPATCH();
                    } else if (!tryStack.empty()) {
                        // The owning handler is in a caller's frame. Leave the
                        // fault in place (pendingException set, running=false) and
                        // return; callFunction() re-raises it from ordinary code.
                        SYNC_IP();
                        return;
                    } else {
                        // Uncaught: runtimeError already printed. Return with the
                        // fault recorded so the caller/driver can see it.
                        SYNC_IP();
                        return;
                    }
                }

                CASE_CODE(ADD_LOCAL_LOCAL) {
                    uint8_t dstSlot = READ_BYTE();
                    uint8_t srcSlot = READ_BYTE();
                    Value& dst = frame->slots[dstSlot];
                    const Value& src = frame->slots[srcSlot];
                    if (__builtin_expect(dst.isInteger() && src.isInteger(), 1)) {
                        dst = Value(wrapAdd(dst.asInteger(), src.asInteger()));
                    } else if (dst.isFloat() && src.isFloat()) {
                        dst = Value(dst.asFloat() + src.asFloat());
                    } else if (dst.isInteger() && src.isFloat()) {
                        dst = Value((double)dst.asInteger() + src.asFloat());
                    } else if (dst.isFloat() && src.isInteger()) {
                        dst = Value(dst.asFloat() + (double)src.asInteger());
                    } else if (dst.isString() && src.isString()) {
                        dst = Value(dst.asString() + src.asString());
                    } else {
                        *stackTop++ = dst;
                        *stackTop++ = src;
                        SYNC_IP();
                        doAdd();
                        if (__builtin_expect(!running, 0)) { RAISE_FAULT(); }
                        LOAD_FRAME();
                        dst = *(--stackTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(SUB_LOCAL_LOCAL) {
                    uint8_t dstSlot = READ_BYTE();
                    uint8_t srcSlot = READ_BYTE();
                    Value& dst = frame->slots[dstSlot];
                    const Value& src = frame->slots[srcSlot];
                    if (__builtin_expect(dst.isInteger() && src.isInteger(), 1)) {
                        dst = Value(wrapSub(dst.asInteger(), src.asInteger()));
                    } else if (dst.isFloat() && src.isFloat()) {
                        dst = Value(dst.asFloat() - src.asFloat());
                    } else if (dst.isInteger() && src.isFloat()) {
                        dst = Value((double)dst.asInteger() - src.asFloat());
                    } else if (dst.isFloat() && src.isInteger()) {
                        dst = Value(dst.asFloat() - (double)src.asInteger());
                    } else {
                        *stackTop++ = dst;
                        *stackTop++ = src;
                        SYNC_IP();
                        doSubtract();
                        if (__builtin_expect(!running, 0)) { RAISE_FAULT(); }
                        LOAD_FRAME();
                        dst = *(--stackTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(INC_LOCAL_BY) {
                    uint8_t dstSlot = READ_BYTE();
                    int8_t imm = static_cast<int8_t>(READ_BYTE());
                    Value& dst = frame->slots[dstSlot];
                    if (__builtin_expect(dst.isInteger(), 1)) {
                        dst = Value(wrapAdd(dst.asInteger(), (long long)imm));
                    } else if (dst.isFloat()) {
                        dst = Value(dst.asFloat() + imm);
                    } else {
                        *stackTop++ = dst;
                        *stackTop++ = Value((long long)imm);
                        SYNC_IP();
                        doAdd();
                        if (__builtin_expect(!running, 0)) { RAISE_FAULT(); }
                        LOAD_FRAME();
                        dst = *(--stackTop);
                    }
                    DISPATCH();
                }

                CASE_CODE(ADD_GLOBAL_LOCAL) {
                    uint16_t globalSlot = READ_SHORT();
                    uint8_t localSlot = READ_BYTE();
                    if (__builtin_expect(globalSlot < globalEnv->globalSlots.size(), 1)) {
                        Value& dst = globalEnv->globalSlots[globalSlot];
                        const Value& src = frame->slots[localSlot];
                        if (__builtin_expect(dst.isInteger() && src.isInteger(), 1)) {
                            dst = Value(wrapAdd(dst.asInteger(), src.asInteger()));
                        } else if (dst.isFloat() && src.isFloat()) {
                            dst = Value(dst.asFloat() + src.asFloat());
                        } else if (dst.isInteger() && src.isFloat()) {
                            dst = Value((double)dst.asInteger() + src.asFloat());
                        } else if (dst.isFloat() && src.isInteger()) {
                            dst = Value(dst.asFloat() + (double)src.asInteger());
                        } else if (dst.isString() && src.isString()) {
                            dst = Value(dst.asString() + src.asString());
                        } else {
                            *stackTop++ = dst;
                            *stackTop++ = src;
                            SYNC_IP();
                            doAdd();
                            if (__builtin_expect(!running, 0)) { RAISE_FAULT(); }
                            LOAD_FRAME();
                            dst = *(--stackTop);
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(PRINT_STR) {
                    Value v = *(--stackTop);
                    if (__builtin_expect(v.isString(), 1)) {
                        std::cout << v.asString() << "\n";
                    } else if (v.isInteger()) {
                        std::cout << v.asInteger() << "\n";
                    } else if (v.isFloat()) {
                        std::cout << v.asFloat() << "\n";
                    } else if (v.isBool()) {
                        std::cout << (v.asBool() ? "true\n" : "false\n");
                    } else if (v.isNil()) {
                        std::cout << "nil\n";
                    } else {
                        std::cout << v.toString() << "\n";
                    }
                    DISPATCH();
                }

                CASE_CODE(INVOKE_METHOD) {
                    {
                        uint16_t nameIdx = READ_SHORT();
                        uint16_t icIdx = READ_SHORT();
                        uint8_t argCount = READ_BYTE();
                        Value* start = stackTop - (argCount + 1);
                        Value receiver = *start;
                        
                        if (__builtin_expect(receiver.isInstance(), 1)) {
                            auto inst = receiver.asInstance();
                            ICCacheEntry& ic = frame->function->chunk.icEntries[icIdx];
                            
                            Value methodVal;
                            bool isGenuineMethod = false;
                            if (__builtin_expect(ic.klass && ic.klass == inst->klass.get(), 1)) {
                                methodVal = ic.methodValue;
                                isGenuineMethod = true;
                            } else {
                                const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                                CHECK_VISIBILITY(inst->klass, propName);
                                bool isField = inst->hasProperty(propName);
                                Value val = inst->getProperty(propName);
                                if (!isField && (val.isFunction() || val.isClosure() || val.isNativeFunction())) {
                                    ic.klass = inst->klass.get();
                                    ic.methodValue = val;
                                    methodVal = val;
                                    isGenuineMethod = true;
                                } else if (!val.isNil()) {
                                    methodVal = val;
                                } else {
                                    Value hook = findGetattrHook(receiver, propName);
                                    if (hook.isCallable()) {
                                        methodVal = Value(std::make_shared<EZBoundMethod>(receiver, hook));
                                    } else {
                                        SYNC_IP();
                                        throwException("AttributeError",
                                            "'" + inst->klass->name + "' has no property or method '" + propName + "'" +
                                            ezDidYouMean(propName, ezInstanceNames(inst)));
                                        RAISE_FAULT();
                                    }
                                }
                            }
                            
                            SYNC_IP();
                            if (isGenuineMethod) {
                                for (int i = (int)argCount; i >= 0; --i) {
                                    start[i + 1] = std::move(start[i]);
                                }
                                *start = methodVal;
                                stackTop++;
                                this->stackTop = stackTop;
                                if (dispatchCall(methodVal, argCount + 1, false, ip)) {
                                    LOAD_FRAME();
                                } else {
                                    REFRESH_FRAME();
                                }
                                stackTop = this->stackTop;
                            } else {
                                *start = methodVal;
                                this->stackTop = stackTop;
                                if (dispatchCall(methodVal, argCount, false, ip)) {
                                    LOAD_FRAME();
                                } else {
                                    REFRESH_FRAME();
                                }
                                stackTop = this->stackTop;
                            }
                        } else if (receiver.isClass()) {
                            auto klass = receiver.asClass();
                            const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                            CHECK_VISIBILITY(klass, propName);
                            Value member = klass->getStaticMember(propName);
                            if (!member.isCallable()) {
                                SYNC_IP();
                                throwException("AttributeError", "Class '" + klass->name + "' has no static method '" + propName + "'");
                                RAISE_FAULT();
                            }
                            *start = member;
                            SYNC_IP();
                            this->stackTop = stackTop;
                            if (dispatchCall(member, argCount, false, ip)) {
                                LOAD_FRAME();
                            } else {
                                REFRESH_FRAME();
                            }
                            stackTop = this->stackTop;
                        } else if (receiver.isDictionary()) {
                            auto dictPtr = receiver.asDictionaryPtr();
                            const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                            Value val = dictPtr->get(propName);
                            if (!val.isCallable()) {
                                SYNC_IP();
                                runtimeError("'" + propName + "' is not callable on dictionary");
                                RAISE_FAULT();
                            }
                            *start = val;
                            SYNC_IP();
                            this->stackTop = stackTop;
                            if (dispatchCall(val, argCount, false, ip)) {
                                LOAD_FRAME();
                            } else {
                                REFRESH_FRAME();
                            }
                            stackTop = this->stackTop;
                        } else {
                            const std::string& propName = std::get<std::string>(frame->function->chunk.getConstant(nameIdx).value);
                            SYNC_IP();
                            runtimeError("Cannot call method '" + propName + "' on " + receiver.typeName());
                            RAISE_FAULT();
                        }
                    }
                    DISPATCH();
                }

                CASE_CODE(END) {
                    running = false;
                    goto end_run;
                }
            } // closes switch body
#if !defined(__GNUC__) || defined(__clang__)
            } // closes while loop body from INTERPRET_LOOP
#endif

            end_run:
            SYNC_IP();
            this->stackTop = stackTop;

    } catch (const RuntimeError& e) {
        if (ownsTryBlock()) {
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
        if (!tryStack.empty()) {
            // The handler belongs to a caller's frame -- see ownsTryBlock().
            // Do NOT rethrow the C++ exception out of this dispatch (it corrupts
            // the unwinder under the event loop). Record it and return; the
            // caller (callFunction) re-raises it toward the owning dispatch.
            pendingException = e.value.isNil() ? Value(e.what()) : e.value;
            SYNC_IP();
            return;
        }
        // Uncaught RuntimeError: already printed in runtimeError() (if not async task)
        pendingException = e.value.isNil() ? Value(e.what()) : e.value;
        SYNC_IP();
    } catch (const std::exception& e) {
        if (!tryStack.empty() && !ownsTryBlock()) {
            // Handler belongs to a caller; hand back via pendingException.
            pendingException = Value(e.what());
            SYNC_IP();
            return;
        }
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
        // Uncaught std exception Ã¢â‚¬â€  format like our runtime errors
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

bool BytecodeVM::dispatchCall(const Value& callee, uint8_t argCount, bool bypassAsyncCheck,
                              const uint8_t* callSiteIp) {
    if (callee.isNativeFunction()) {
        auto native = callee.asNativeFunction();
        int arity = native->arity;

        // Arity gate: fixed-arity natives (arity >= 0) must receive AT LEAST
        // that many arguments. Variadic natives (arity == -1) skip the check.
        // We check `<` instead of `!=` because native methods (BoundMethods)
        // have `self` injected into `argCount` by the VM, making `argCount`
        // larger than the user-facing `arity`.
        // This still prevents the UB crash where a call site provides too few
        // arguments, causing `args[N]` to read out of bounds.
        if (arity >= 0 && static_cast<int>(argCount) < arity) {
            runtimeError(native->name + "() expected " + std::to_string(arity) +
                         " argument(s), got " + std::to_string(argCount));
            return false;
        }

        // Collect args from stack (they sit above the callee)
        Value* oldTop = stackTop;
        std::vector<Value> args(stackTop - argCount, stackTop);

        try {
            Value result = callee.asNativeFunction()->function(*this, args);
            // NOW we pop everything and push the result
            stackTop -= argCount + 1;
            // Release the vacated callee + argument slots so their (possibly
            // heap-object) copies don't linger above the stack top and defeat
            // the GC. `args`/`result` hold independent copies, so this is safe.
            clearStackSlots(stackTop, oldTop);
            push(result);
        } catch (const RuntimeError& e) {
            // A native that reports failure through interp.runtimeError() no
            // longer reaches here at all: the dispatch is live, so runtimeError()
            // records the fault and returns, and the native's own `return
            // Value()` brings us back normally. This catch is the backstop for a
            // native that constructs and throws a RuntimeError itself.
            //
            // Either way the fault must NOT be re-thrown. Re-throwing was what
            // made an ordinary defensive helper fatal inside a request handler:
            //   task safe(x) { try { give parse_json(x) } catch (e) { give nil } }
            // parse_json("5") raises "Failed to parse JSON", and the C++ throw
            // could not unwind under the libuv callback, so the process died
            // instead of the EZ catch running.
            if (pendingException.isNil()) {
                pendingException = e.value.isNil() ? Value(e.what()) : e.value;
            }
            running = false;
            return false;
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
        
        // dispatchCall takes a uint8_t argCount, so argCount+1 would wrap to 0 at
        // 255 args and silently drop every argument.
        if (argCount >= 255) {
            runtimeError("Too many arguments to method call (max 254)");
            return false;
        }

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
            if (ezFunc) {
                std::shared_lock<std::shared_mutex> lk(globalEnv->registryMutex);
                auto it = globalEnv->compiledFunctionCache.find(ezFunc.get());
                if (it != globalEnv->compiledFunctionCache.end()) {
                    bcFunc = it->second;
                } else {
                    lk.unlock();
                    bcFunc = compileEZFunction(ezFunc.get());
                }
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
            
            // The parameter list is the answer to "what should I have passed",
            // so print it rather than only a count. A signature makes the fix
            // obvious; "expected at least 2 args but got 1" does not say which
            // one is missing.
            auto signature = [&]() {
                std::string out = bcFunc->name + "(";
                size_t start = bcFunc->isMethod ? 1 : 0;   // self is not the caller's to pass
                // Unsigned: a defaultParamCount above arity would wrap to a huge
                // number and mark nothing optional, so clamp rather than subtract.
                size_t firstOptional = bcFunc->defaultParamCount >= bcFunc->arity
                                     ? 0 : bcFunc->arity - bcFunc->defaultParamCount;
                for (size_t i = start; i < bcFunc->paramNames.size(); ++i) {
                    if (i > start) out += ", ";
                    out += bcFunc->paramNames[i];
                    if (i >= firstOptional) out += " = ...";
                }
                return out + ")";
            };

            if (argCount < minArity) {
                std::string message = "'" + bcFunc->name + "' needs " +
                    std::to_string(reportedMinArity) +
                    (reportedMinArity == 1 ? " argument" : " arguments") + ", got " +
                    std::to_string(reportedArgCount);
                if (!bcFunc->paramNames.empty()) message += "\n  Signature: " + signature();
                throwException("TypeError", message);
                return false;
            }
            if (argCount > bcFunc->arity) {
                std::string message = "'" + bcFunc->name + "' takes at most " +
                    std::to_string(reportedArity) +
                    (reportedArity == 1 ? " argument" : " arguments") + ", got " +
                    std::to_string(reportedArgCount);
                if (!bcFunc->paramNames.empty()) message += "\n  Signature: " + signature();
                throwException("TypeError", message);
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
            Value closedFunc = callee;
            bool shouldTrace = this->traceExecution;
            
            EventLoop::instance().pushTask([ezFut, globalEnv, closedFunc, closedArgs, shouldTrace]() {
                try {
                    auto taskVM = std::make_shared<BytecodeVM>(globalEnv, 8192);
                    taskVM->taskFuture = ezFut;
                    taskVM->traceExecution = shouldTrace;
                    taskVM->isAsyncTask = true;
                    taskVM->push(closedFunc);
                    for (auto& a : closedArgs) taskVM->push(a);
                    
                    if (taskVM->dispatchCall(closedFunc, closedArgs.size(), true)) {
                        taskVM->isYielded = false;
                        taskVM->run(0); // run until completion or yield
                        
                        if (!taskVM->isYielded) {
                            if (taskVM->isExceptionPending || !taskVM->pendingException.isNil()) {
                                std::string errMsg = !taskVM->pendingException.isNil() ? taskVM->pendingException.toString() : "Async task failed with an exception";
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

        // Scoped to the executing constructor, not to the instance: in a chain
        // three deep each level legitimately calls super() once on the SAME
        // instance, so an instance-wide flag rejected the second level.
        if (!frames.empty()) {
            if (frames.back().superCalled) {
                runtimeError("super() has already been called");
                return false;
            }
            frames.back().superCalled = true;
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

    // Name what was called. The bare form -- "Value is not callable: nil" --
    // says nothing about WHICH call failed, and on a line with three of them
    // the reader has to guess. The compiler recorded the source spelling of
    // every call site, keyed by the offset frame->ip is sitting on right now.
    std::string what = calleeNameAtCallSite(callSiteIp);
    std::string detail;

    if (what.empty()) {
        detail = "cannot call a " + callee.typeName() + " value";
    } else if (callee.isNil()) {
        // By far the commonest case, and the type alone is actively
        // misleading: it reads as "the function returned nil" when what
        // happened is that the name never held a function.
        detail = "'" + what + "' is nil, so it cannot be called";
        if (what.find('.') != std::string::npos) {
            detail += "\n  Hint: check the spelling, and that the object has that method.";
        } else {
            detail += "\n  Hint: '" + what + "' is not defined, or was assigned a"
                      " non-function value that replaced it.";
        }
    } else {
        detail = "'" + what + "' is a " + callee.typeName() + ", not a function";
    }
    throwException("TypeError", detail);
    return false;
}

// The source spelling of the call at `callSiteIp`, or "".
//
// The pointer is the one the call opcode held just past the instruction, which
// is exactly the key the compiler recorded.
std::string BytecodeVM::calleeNameAtCallSite(const uint8_t* callSiteIp) const {
    if (!callSiteIp || frames.empty()) return "";
    const CallFrame& frame = frames.back();
    if (!frame.function || frame.function->callSites.empty()) return "";

    const std::vector<uint8_t>& code = frame.function->chunk.code;
    if (code.empty()) return "";
    const uint8_t* base = code.data();
    // The pointer must belong to THIS frame's code. A mismatch means the call
    // already pushed a frame before failing, and indexing another function's
    // table would name something unrelated.
    if (callSiteIp < base || callSiteIp > base + code.size()) return "";

    uint32_t offset = static_cast<uint32_t>(callSiteIp - base);
    if (const std::string* name = frame.function->calleeNameAt(offset)) return *name;
    return "";
}

void BytecodeVM::pushCallFrame(BytecodeFunctionPtr bcFunc, uint8_t argCount, ClosureState cs) {
    // Enforce maximum call depth to catch unbounded recursion cleanly
    if (frames.size() >= FRAMES_MAX) {
        throwException("RecursionError",
            "maximum call depth of " + std::to_string(FRAMES_MAX) + " exceeded"
            "\n  Hint: usually a recursive task with no base case, or one whose"
            "\n  base case is never reached.");
        return;
    }

    // Guard the fast-path frame-setup pushes below (which bypass push()'s own
    // bounds check) against overrunning the operand-stack buffer. The caller
    // (CALL/dispatchCall) has already synced this->stackTop, so it reflects the
    // live top here. We must have room for any optional-parameter padding, all
    // local slots, plus the working headroom the function body's expression
    // temporaries will need.
    if (!hasStackHeadroom(bcFunc->localCount + STACK_HEADROOM)) {
        runtimeError("Stack overflow: operand stack exhausted");
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
    if (stackTop >= stack.data() + stackMax) {
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
    --stackTop;
    Value v = std::move(*stackTop);
    *stackTop = Value(); // release the vacated slot's reference (see clearStackSlots)
    return v;
}

Value& BytecodeVM::peek(int distance) {
    return stackTop[-1 - distance];
}

void BytecodeVM::popN(size_t count) {
    while (count-- > 0) {
        --stackTop;
        *stackTop = Value(); // release each vacated slot's reference
    }
}

// ============================================================================
// Upvalue Handling
// ============================================================================

std::shared_ptr<UpvalueObj> BytecodeVM::captureUpvalue(Value* local) {
    // Walk the open upvalue list to find an existing capture
    std::shared_ptr<UpvalueObj> prev = nullptr;
    std::shared_ptr<UpvalueObj> cur  = openUpvalues;
    while (cur != nullptr && cur->location.load() > local) {
        prev = cur;
        cur  = cur->next;
    }
    if (cur != nullptr && cur->location.load() == local) return cur;

    // Create a new open upvalue, co-owned by the open list (and, once the
    // CLOSURE opcode stores it, by the closure).
    auto uv = std::make_shared<UpvalueObj>();
    uv->location.store(local);
    uv->next = cur;
    if (prev == nullptr) openUpvalues = uv;
    else prev->next = uv;
    return uv;
}

void BytecodeVM::doAdd() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(wrapAdd(a.asInteger(), b.asInteger()))); return; }
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
        auto res = a.asArray().getElementsCopy();
        for (const Value& v : b.asArray().getElementsCopy()) res.push_back(v);
        push(Value::makeArrayCopy(res));
        return;
    }
    runtimeError("'+' operands must be numbers, strings, or arrays");
}

void BytecodeVM::doSubtract() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(wrapSub(a.asInteger(), b.asInteger()))); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   - b.asFloat()));   return; }
    runtimeError("'-' operands must be numbers");
}

void BytecodeVM::doMultiply() {
    Value b = pop(), a = pop();
    if (a.isInteger() && b.isInteger()) { push(Value(wrapMul(a.asInteger(), b.asInteger()))); return; }
    if (a.isNumber()  && b.isNumber())  { push(Value(a.asFloat()   * b.asFloat()));   return; }
    // "str" * N  repetition
    if (a.isString() && b.isInteger()) {
        long long count = b.asInteger();
        if (count < 0) { runtimeError("String repeat count must not be negative"); return; }
        // Refuse absurd repetitions rather than trying to allocate them.
        const long long kMaxRepeatBytes = 64LL * 1024 * 1024;
        long long unit = (long long)a.stringLength();
        if (unit > 0 && count > kMaxRepeatBytes / unit) {
            runtimeError("String repeat result too large");
            return;
        }
        std::string result;
        result.reserve((size_t)(unit * count));
        std::string s = a.asString();
        for (long long i = 0; i < count; i++) result += s;
        push(Value(result));
        return;
    }
    runtimeError("'*' operands must be numbers");
}

void BytecodeVM::doDivide() {
    Value b = pop(), a = pop();
    if (b.isInteger() && b.asInteger() == 0) { throwException("ZeroDivisionError", "division by zero"); return; }
    if (b.isFloat()   && b.asFloat()   == 0) { throwException("ZeroDivisionError", "division by zero"); return; }
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
        if (b.asInteger() == 0) { throwException("ZeroDivisionError", "modulo by zero"); return; }
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
    // Wrapping square-and-multiply for integer bases with non-negative exponents,
    // consistent with the language's defined-wrap integer semantics.
    if (a.isInteger() && b.isInteger() && b.asInteger() >= 0) {
        long long base = a.asInteger();
        long long exp = b.asInteger();
        long long result = 1;
        while (exp > 0) {
            if (exp % 2 == 1) result = wrapMul(result, base);
            exp /= 2;
            if (exp > 0) base = wrapMul(base, base);
        }
        push(Value(result));
        return;
    }
    if (a.isNumber() && b.isNumber()) {
        push(Value(std::pow(a.asFloat(), b.asFloat())));
        return;
    }
    runtimeError("'**' operands must be numbers");
}

void BytecodeVM::doNegate() {
    Value a = pop();
    // -LLONG_MIN is not representable (UB); wrapNeg gives the defined
    // two's-complement result, consistent with the language's wrapping integers.
    if (a.isInteger()) { push(Value(wrapNeg(a.asInteger()))); return; }
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
        // A shift count outside [0,63] is undefined behaviour, as is left-shifting
        // a negative value; reject the former and shift as unsigned for the latter.
        long long shift = b.asInteger();
        if (shift < 0 || shift > 63) { throwException("ValueError", "a shift amount must be between 0 and 63"); return; }
        unsigned long long ua = (unsigned long long)a.asInteger();
        push(Value((long long)(ua << shift)));
        return;
    }
    runtimeError("'<<' operands must be numbers");
}
void BytecodeVM::doShiftRight() {
    Value b = pop(), a = pop();
    if (a.isNumber() && b.isNumber()) {
        long long shift = b.asInteger();
        if (shift < 0 || shift > 63) { throwException("ValueError", "a shift amount must be between 0 and 63"); return; }
        push(Value(a.asInteger() >> shift));
        return;
    }
    runtimeError("'>>' operands must be numbers");
}

void BytecodeVM::guardedHelper(void (BytecodeVM::*fn)()) {
    // The helper runs inside run()'s dispatch, so runtimeError() already records
    // faults as running=false + pendingException and returns rather than
    // throwing -- see dispatchDepth_ in BytecodeVM.h. The DISPATCH macro's
    // `running` check then routes to handle_vm_fault.
    //
    // The try/catch is a backstop for a C++ exception raised by something that
    // does NOT go through runtimeError (std::bad_alloc, a throwing std:: call in
    // a helper). The normal fault path raises nothing to catch.
    try {
        (this->*fn)();
    } catch (const RuntimeError& e) {
        if (pendingException.isNil()) {
            pendingException = e.value.isNil() ? Value(e.what()) : e.value;
        }
        running = false;
    } catch (const std::exception& e) {
        if (pendingException.isNil()) pendingException = Value(std::string(e.what()));
        running = false;
    }
}

void BytecodeVM::doIndexGet() {
    Value idx = pop();
    Value obj = pop();
    // Sequence types require a numeric index. Without this check asInteger() on a
    // nil/string/object index logs to stderr and throws bad_variant_access rather
    // than producing a clean, catchable EZ error.
    if ((obj.isArray() || obj.isTuple() || obj.isString() || obj.isBuffer()) && !idx.isNumber()) {
        throwException("TypeError",
            "an array index must be a number, got a " + idx.typeName() + " value");
        return;
    }
    if (obj.isArray()) {
        auto& arr = obj.asArray();
        long long i = idx.asInteger();
        if (!checkBounds(i, arr.size(), "array")) return;
        push(arr[i]);
    } else if (obj.isTuple()) {
        auto& tup = obj.asTuple();
        long long i = idx.asInteger();
        if (!checkBounds(i, tup.size(), "tuple")) return;
        push(tup[i]);
    } else if (obj.isString()) {
        const std::string& s = obj.asString();
        long long i = idx.asInteger();
        if (!checkBounds(i, s.length(), "string")) return;
        push(Value(std::string(1, s[i])));
    } else if (obj.isDictionary()) {
        // Single locked O(1) lookup. This used to copy the WHOLE map
        // (getMapCopy()) just to read one key, making every dict[key] O(n) --
        // with a string copy and an atomic refcount bump per copied entry.
        push(obj.asDictionaryPtr()->get(idx.toString()));
    } else if (obj.isBuffer()) {
        auto& buf = obj.asBuffer();
        long long i = idx.asInteger();
        if (!checkBounds(i, buf.size(), "buffer")) return;
        push(Value((long long)buf[i]));
    } else {
        throwException("TypeError",
            "a " + obj.typeName() + " value cannot be indexed"
            "\n  Hint: [] works on arrays, dictionaries, strings and buffers.");
    }
}

void BytecodeVM::doIndexSet() {
    Value val = pop();
    Value idx = pop();
    Value obj = pop();
    if ((obj.isArray() || obj.isBuffer()) && !idx.isNumber()) {
        throwException("TypeError",
            "an array index must be a number, got a " + idx.typeName() + " value");
        return;
    }
    if (obj.isArray()) {
        auto& arr = obj.asArray();
        long long i = idx.asInteger();
        if (i < 0) { runtimeError("Array index out of bounds"); return; }
        // Assigning far past the end would silently allocate a huge array.
        const long long kMaxGrow = 64LL * 1024 * 1024;
        if (i >= kMaxGrow) { runtimeError("Array index too large"); return; }
        if (i >= (long long)arr.size()) arr.resize(i + 1);
        arr.set(i, val);
    } else if (obj.isDictionary()) {
        auto dictPtr = obj.asDictionaryPtr();
        dictPtr->set(idx.toString(), val);
    } else if (obj.isBuffer()) {
        auto& buf = obj.asBuffer();
        long long i = idx.asInteger();
        if (!checkBounds(i, buf.size(), "buffer")) return;
        if (!val.isNumber()) { runtimeError("Buffer value must be a number, got " + val.typeName()); return; }
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

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

// ============================================================================
// Error Handling
// ============================================================================

