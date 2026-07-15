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
        std::vector<UpvalueObj*> upvalues;
    };
    
    void adoptUpvalue(std::unique_ptr<struct UpvalueObj> uv) {
        allUpvalues.push_back(std::move(uv));
    }

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
    static constexpr size_t FRAMES_MAX = 512;
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
    
    // Upvalue management
    UpvalueObj*                              openUpvalues; // sorted linked list
    std::vector<std::unique_ptr<UpvalueObj>> allUpvalues;  // owns all UpvalueObjs

    // Try/catch stack
    std::vector<TryBlock> tryStack;
    Value                 pendingException;
    bool                  isExceptionPending;

    // Execution flag
    bool running;

    // Global Slot Array moved to Environment for async task sharing

    // ── Shared registries moved to Environment ─────────────────────────

    // ── Core ──────────────────────────────────────────────────────────────────
    void run(size_t targetFrameCount = 0);
    bool dispatchCall(const Value& callee, uint8_t argCount, bool bypassAsyncCheck = false);
    void pushCallFrame(BytecodeFunctionPtr bcFunc, uint8_t argCount, ClosureState cs);

    // ── Stack ─────────────────────────────────────────────────────────────────
    void   push(const Value& value);
    Value  pop();
    Value& peek(int distance = 0);
    void   popN(size_t count);

    // ── Upvalues ─────────────────────────────────────────────────────────────
    UpvalueObj* captureUpvalue(Value* local);
    void        closeUpvalues(Value* last);

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

    // ── Utility ──────────────────────────────────────────────────────────────
    void               initBuiltins();
    BytecodeFunctionPtr compileEZFunction(EZFunction* func);
    inline bool checkBounds(long long index, size_t size, const char* errorMsg) {
        if (index < 0 || index >= (long long)size) {
            runtimeError(errorMsg);
            return false;
        }
        return true;
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
