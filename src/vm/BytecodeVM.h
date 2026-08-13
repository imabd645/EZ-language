#include "runtime/objects/EZUpvalue.h"
#ifndef BYTECODE_VM_H
#define BYTECODE_VM_H

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <deque>
#include <regex>
#include "bytecode/Bytecode.h"
#include "compiler/BytecodeCompiler.h"
#include "runtime/Value.h"
#include "runtime/Environment.h"
#include "runtime/RuntimeContext.h"
#include "gc/CycleCollector.h"

// ============================================================================
// Bytecode Virtual Machine - Stack-based execution
// ============================================================================

// Register a source file's text into the global source registry so that
// runtimeError() can display the offending source line as a snippet.
// Source registry functions
void EZ_RegisterSource(const std::string& filename, const std::string& source);
const std::string* EZ_GetSourceLine(const std::string& filename, int line);

class BytecodeVM : public RuntimeContext, public std::enable_shared_from_this<BytecodeVM> {
public:
    bool traceExecution = false;
    BytecodeVM(size_t stackSize = 256 * 1024);
    explicit BytecodeVM(std::shared_ptr<Environment> globalEnv, size_t stackSize = 256 * 1024);
    ~BytecodeVM();

    // Execute compiled bytecode
    Value execute(BytecodeFunctionPtr function);
    Value execute(BytecodeFunctionPtr function, const std::vector<Value>& args);

    // Call a Value (function/native) from external code
    Value callFunction(const Value& callee, const std::vector<Value>& args, int line = 0, const std::string& filename = "native") override;
    void printStackTrace() const override;

    // RuntimeContext interface
    void runtimeError(const std::string& message, int line = 0, const std::string& filename = "") override;

    void throwException(const std::string& className, const std::string& message, int line = 0, const std::string& filename = "") override;
    std::shared_ptr<Environment> getGlobalEnv() override { return globalEnv; }
    void defineGlobal(const std::string& name, const Value& value) override;
    Value eval(const std::string& code, const std::string& filename = "<eval>") override;
    std::string stringify(const Value& val, int line = 0, const std::string& filename = "") override;

    // Clear the unused tail of the value stack. Everything at or above stackTop
    // is dead by definition -- the program cannot reach it -- but the slots
    // still hold their last Value and keep those objects alive.
    void releaseStaleStackSlots() override {
        if (!stack.empty() && stackTop >= stack.data()) {
            clearStackSlots(stackTop, stack.data() + stack.size());
        }
    }
    std::shared_ptr<Environment> getCurrentEnv() const override { return globalEnv; }

    // Initialize global slot table from compiler output (Issue C optimization)
    // Seeds globalSlots[] from CompileResult and pre-populates any
    // built-ins already stored in globalEnv.
    void initGlobalSlots(const std::vector<std::string>& slotNames);

    // Stack depth query
    size_t getStackSize() const {
        return static_cast<size_t>(stackTop - stack.data());
    }

    struct ClosureState {
        std::vector<std::shared_ptr<UpvalueObj>> upvalues;
    };

    const std::vector<std::string>& getGlobalSlotNames() const { return globalEnv->globalSlotNames; }
    const std::vector<Value>& getGlobalSlots() const { return globalEnv->globalSlots; }


    bool isYielded = false;
    bool isAsyncTask = false;
    std::shared_ptr<EZFuture> taskFuture;

private:
    // ── Call Frame ────────────────────────────────────────────────────────────
    struct CallFrame {
        BytecodeFunctionPtr function;
        const uint8_t* ip;    // instruction pointer into chunk.code
        Value*         slots; // base stack pointer (arg0 is slots[0])
        size_t         localCount;
        std::string    functionName;
        std::string    filename;   // Source file this frame belongs to
        int            line;
        // Has this invocation already called super()? The rule is one super()
        // per CONSTRUCTOR CALL, so it belongs on the frame. Tracking it on the
        // instance instead broke every inheritance chain three deep or more:
        // GrandChild.init calls super(), which runs Child.init, which calls
        // super() on the same instance and was rejected as a duplicate.
        bool           superCalled = false;

        const Chunk& chunk() const { return function->chunk; }
    };

    // ── Exception handling ────────────────────────────────────────────────────
    struct TryBlock {
        size_t          frameIdx;
        const uint8_t*  catchIp;
        const uint8_t*  finallyIp;
        Value*          stackTop;
    };

    // ── VM Configuration ──────────────────────────────────────────────────────
    size_t                 stackMax;

    // Maximum EZ call depth.
    //
    // Was 512, which is low enough to hit doing ordinary work: walking a deep
    // tree, or any recursive-descent parse, dies well before the algorithm is
    // at fault. Python allows 1000 by default and lets you raise it.
    //
    // Raising this is cheap because an EZ frame is an element of the `frames`
    // vector on the heap, not a native stack frame -- run() is a flat dispatch
    // loop, so EZ-to-EZ recursion does not consume the C stack at all. What it
    // does consume is operand-stack slots for each frame's locals, and that is
    // already bounded separately: pushCallFrame checks STACK_HEADROOM against
    // stackMax and reports a clean overflow, so a function with an unusually
    // large frame still degrades to an error rather than corrupting anything.
    //
    // (Re-entrant paths -- constructors and FFI callbacks, which go through
    // callFunction and nest a real run() -- are still bounded by the native
    // stack. Those nest a handful deep in practice, not thousands.)
    static constexpr size_t FRAMES_MAX = 4096;
    // Working-stack reserve required to be free when a new call frame is
    // installed. Opcode operands are uint8_t-bounded (arg/element counts <= 255)
    // and the compiler never emits an unboundedly deep single expression, so this
    // margin conservatively covers a frame's intra-body expression temporaries.
    static constexpr size_t STACK_HEADROOM = 1024;
    std::vector<Value>     stack;
    Value*                 stackTop;

    std::vector<CallFrame>    frames;
    std::vector<ClosureState> frameUpvalues;

    std::shared_ptr<Environment> globalEnv;

    // BytecodeFunction cache moved to Environment
    
    // Upvalue management. The open list is a shared_ptr chain so an open upvalue
    // stays alive even if the only closure that captured it is destroyed while
    // the upvalue is still open. Closures co-own their captured upvalues, so a
    // closure outliving this VM keeps its upvalues valid.
    std::shared_ptr<UpvalueObj> openUpvalues; // sorted linked list head

    // Try/catch stack
    std::vector<TryBlock> tryStack;
    Value                 pendingException;
    bool                  isExceptionPending;

    // Execution flag
    bool running;

    // ── The no-throw-inside-dispatch invariant ────────────────────────────────
    //
    // How deep we are inside run()'s computed-goto dispatch. Non-zero means the
    // dispatch is on the C++ stack, and in that state THE VM MUST NOT THROW.
    //
    // A C++ exception cannot unwind out of the dispatch when run() is itself
    // executing inside the libuv event-loop callback -- which is the case for
    // every FFI callback and every ezweb request handler. Verified with gdb: the
    // throw skips every catch on the stack (the helper, run(), callFunction, the
    // uv callback) and jumps to a null handler, so the process dies with an
    // access violation instead of the error being caught.
    //
    // Faults therefore travel as `pendingException` + `running = false`. The
    // DISPATCH() macro notices `!running` and jumps to handle_vm_fault, which
    // either resumes in the owning EZ catch block, or returns so that an outer
    // frame can. runtimeError() and throwException() consult this counter and
    // record-and-return rather than throw whenever it is non-zero; they still
    // throw when called from outside the dispatch, where unwinding is the
    // correct way to reach an embedding caller.
    //
    // This replaced a series of one-site patches (a faultMode flag around the
    // arithmetic helpers, then a separate non-throwing path for dispatchCall,
    // then another for native functions). Each covered a single call site and
    // the next unguarded site became the next crash; the counter states the rule
    // once, for every fault the VM can raise.
    int dispatchDepth_ = 0;

    // Increments dispatchDepth_ for the lifetime of a scope. Uses RAII so the
    // count is restored even if some path further in does unwind -- a leaked
    // increment would silently disable throwing for the rest of the program.
    struct DispatchScope {
        BytecodeVM* vm;
        explicit DispatchScope(BytecodeVM* v) : vm(v) { ++vm->dispatchDepth_; }
        ~DispatchScope() { --vm->dispatchDepth_; }
        DispatchScope(const DispatchScope&) = delete;
        DispatchScope& operator=(const DispatchScope&) = delete;
    };

public:
    // True while run()'s dispatch is on the stack. Callers that are about to
    // raise a fault use it to choose between recording and throwing.
    bool insideDispatch() const { return dispatchDepth_ > 0; }
private:

    // Global Slot Array moved to Environment for async task sharing

    // ── Shared registries moved to Environment ─────────────────────────

    // ── Core ──────────────────────────────────────────────────────────────────
    void run(size_t targetFrameCount = 0);
    // `callSiteIp` is the instruction pointer just past the call instruction,
    // and is supplied ONLY by the bytecode call opcodes. It is how a failed
    // call names itself.
    //
    // The internal callers -- operator overloads, getattr hooks, native code
    // calling back through callFunction -- pass nothing, deliberately. Their
    // caller's ip belongs to a different call: a builtin like map(arr, fn)
    // reaching here with a nil `fn` would otherwise report "'map' is nil",
    // naming the one function on that line which is fine.
    bool dispatchCall(const Value& callee, uint8_t argCount, bool bypassAsyncCheck = false,
                      const uint8_t* callSiteIp = nullptr);

    // The source spelling of the call at `callSiteIp` ("readFile", "user.save"),
    // or "" when there is none to report.
    std::string calleeNameAtCallSite(const uint8_t* callSiteIp) const;
    void pushCallFrame(BytecodeFunctionPtr bcFunc, uint8_t argCount, ClosureState cs);

    // ── Stack ─────────────────────────────────────────────────────────────────
    void   push(const Value& value);
    Value  pop();
    Value& peek(int distance = 0);
    void   popN(size_t count);

    // ── Upvalues ─────────────────────────────────────────────────────────────
    std::shared_ptr<UpvalueObj> captureUpvalue(Value* local);
    void                        closeUpvalues(Value* last);

    // Run a doXXX helper and, if it raises a RuntimeError, catch it HERE -- in
    // this plain function's frame, which has correct exception unwind info --
    // rather than letting the C++ exception unwind through run()'s computed-goto
    // dispatch, whose goto*-based control flow corrupts the landing-pad tables so
    // the catch is skipped and the process crashes (fatally under the libuv event
    // loop that drives every FFI callback and ezweb request handler). The fault
    // becomes running=false + pendingException, which the dispatch checks.
    //
    // MUST NOT be inlined: the whole point is that the try/catch lives in a
    // SEPARATE frame from run()'s computed-goto dispatch. If -O3/LTO inlined it
    // into run(), the catch would move back into the region with the corrupt
    // landing-pad tables and the fix would silently stop working.
#if defined(__GNUC__)
    __attribute__((noinline))
#endif
    void guardedHelper(void (BytecodeVM::*fn)());

    // ── Arithmetic ───────────────────────────────────────────────────────────
    void doAdd(); void doSubtract(); void doMultiply(); void doDivide();
    void doModulo(); void doPower(); void doNegate();
    void doBitwiseAnd(); void doBitwiseOr(); void doBitwiseXor();
    void doBitwiseNot(); void doShiftLeft(); void doShiftRight();

    // ── Comparisons ──────────────────────────────────────────────────────────
    void doEqual(); void doNotEqual();
    void doLess(); void doLessEq(); void doGreater(); void doGreaterEq();
    void doNot();
    void doIndexGet(); void doIndexSet();

    // ── Attribute hooks (__getattr__ / __setattr__) ──────────────────────────
    //
    // A class may intercept property access it would otherwise fail on:
    //
    //   task __getattr__(name)         -- called when a property is NOT found
    //   task __setattr__(name, value)  -- called on every property write
    //
    // __getattr__ sits on the inline-cache MISS path only, so an ordinary hit
    // still goes through the shape/class IC and costs nothing. __setattr__ has
    // to see every write to be useful (it exists so an object can know what
    // changed), so it is checked before the store fast path.
    //
    // The raw, un-hooked accessors are the getattr()/setattr() builtins: they
    // call EZInstance::getProperty/setProperty directly rather than going
    // through these opcodes. That is the escape hatch a hook body needs in order
    // to actually read or write without re-entering itself.
    //
    // These only LOOK the hook up; the call itself is issued inline at each use
    // site with dispatchCall(), the same way an overloaded operator is invoked.
    // That pushes a frame for the main loop to run instead of re-entering run(),
    // so a throw inside a hook unwinds through ordinary bytecode rather than
    // through a nested C++ interpreter activation.
    //
    // Returns a nil Value when the class defines no hook.
    Value findGetattrHook(const Value& obj, const std::string& name);
    Value findSetattrHook(const Value& obj, const std::string& name);

    // ── Utility ──────────────────────────────────────────────────────────────
    void               initBuiltins();
    BytecodeFunctionPtr compileEZFunction(EZFunction* func);
    // `what` names the container kind ("array", "string", …) and appears in the
    // message, which reports the index AND the length. "Array index out of
    // bounds" on its own leaves the reader to go and find out how long the
    // array actually was, and off-by-one is the usual cause -- seeing "index 5,
    // length 5" answers it on the spot.
    inline bool checkBounds(long long index, size_t size, const char* what) {
        if (index >= 0 && index < (long long)size) return true;

        std::string message = std::string(what) + " index " + std::to_string(index) +
                              " is out of range";
        if (size == 0) {
            message += ": it is empty";
        } else {
            message += ": valid indices are 0 to " + std::to_string(size - 1) +
                       " (length " + std::to_string(size) + ")";
        }
        if (index < 0) {
            message += "\n  Hint: EZ has no negative indexing -- use "
                       "x[len(x) - 1] for the last element.";
        }
        throwException("IndexError", message);
        return false;
    }

    // Returns true if at least `extra` more Value slots can be pushed onto the
    // operand stack without overrunning the backing buffer. Used to guard the
    // fast-path frame-setup pushes that bypass push()'s own bounds check.
    inline bool hasStackHeadroom(size_t extra) const {
        return (size_t)(stackTop - stack.data()) + extra <= stackMax;
    }

    // Reset the operand-stack slots in [from, to) back to NIL, releasing any
    // shared_ptr references they hold. Abandoned slots left above the stack top
    // would otherwise keep dead objects alive (a leak) and inflate their
    // shared_ptr use_count, which makes the cycle collector treat genuine
    // garbage as externally reachable and never reclaim it.
    inline void clearStackSlots(Value* from, Value* to) {
        for (Value* p = from; p < to; ++p) *p = Value();
    }

    void               printStack() const;
    Value              instantiate(std::shared_ptr<EZClass> klass, const std::vector<Value>& args, int line, const std::string& filename);
};

#endif // BYTECODE_VM_H
