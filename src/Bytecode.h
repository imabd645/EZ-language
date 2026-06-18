#ifndef BYTECODE_H
#define BYTECODE_H

#include <cstdint>
#include <vector>
#include <string>
#include <variant>
#include <memory>
#include "Value.h"

// ============================================================================
// EZ Bytecode Instruction Set (Stack-based VM)
// ============================================================================

enum class OpCode : uint8_t {
    // Constants & Variables
    LOAD_CONST = 0,      // idx: Load constant from pool
    LOAD_LOCAL,          // idx: Load local variable
    STORE_LOCAL,         // idx: Store to local variable
    LOAD_UPVALUE,        // idx: Load upvalue
    STORE_UPVALUE,       // idx: Store upvalue
    LOAD_GLOBAL,         // name_idx: Load global
    STORE_GLOBAL,        // name_idx: Store global
    LOAD_PROPERTY,       // name_idx: Load property
    STORE_PROPERTY,      // name_idx: Store property
    POP,                 // Pop top of stack
    DUP,                 // Duplicate top of stack
    DUP2,                // Duplicate top 2 elements
    
    // Constants (inline for common values)
    LOAD_NIL,            // Push nil
    LOAD_TRUE,           // Push true
    LOAD_FALSE,          // Push false
    LOAD_ZERO,           // Push 0
    LOAD_ONE,            // Push 1
    LOAD_EMPTY_STR,      // Push ""
    
    // Local variable ops (optimized)
    INC_LOCAL,           // idx: local[idx]++ (integer fast path)
    DEC_LOCAL,           // idx: local[idx]-- (integer fast path)
    
    // Arithmetic
    ADD,
    SUB,
    MUL,
    DIV,
    MOD,
    POW,
    NEGATE,
    
    // Bitwise
    BIT_AND,
    BIT_OR,
    BIT_XOR,
    BIT_NOT,
    SHIFT_LEFT,
    SHIFT_RIGHT,
    
    // Comparisons
    EQUAL,
    NOT_EQUAL,
    LESS,
    LESS_EQ,
    GREATER,
    GREATER_EQ,
    
    // Logical
    NOT,
    
    // Control Flow
    JUMP,                // offset: Unconditional jump
    JUMP_IF_FALSE,       // offset: Jump if false
    JUMP_IF_TRUE,        // offset: Jump if true
    LOOP,                // offset: Jump backward (for loops)
    
    // Functions
    CALL,                // arg_count: Call function
    TAIL_CALL,           // arg_count: Tail call optimization
    RETURN,              // Return from function
    CLOSURE,             // const_idx: Create closure
    CLOSE_UPVALUE,       // Close upvalues >= idx
    
    // Collections
    MAKE_ARRAY,          // count: Create array from stack values
    MAKE_DICT,           // count: Create dict from stack pairs
    INDEX_GET,           // Get arr[idx] or obj[key]
    INDEX_SET,           // Set arr[idx] = val
    ARRAY_APPEND,        // [array, val] -> [array] (appends val)
    ARRAY_EXTEND,        // [array, iterable] -> [array] (extends with iterable)
    CALL_SPREAD,         // [callee, array] -> Call function with array elements
    // Models/Objects
    NEW_INSTANCE,        // const_idx: Create new model instance
    GET_METHOD,          // name_idx: Get method binding
    SUPER,               // Pushes Super object
    SUPER_CALL,          // Call parent method
    
    // Iterators
    GET_ITER,            // Get iterator from iterable
    GET_DICT_ITER,       // Get KV iterator from dictionary
    ITER_NEXT,           // offset: Next value or jump
    ITER_HAS_NEXT,       // Check if iterator has more
    
    // Exception handling
    TRY_START,           // catch_offset, finally_offset
    TRY_END,             // End try block
    THROW,               // Throw exception
    
    // Built-ins
    PRINT,               // Print top of stack
    CLOCK,               // Push current time
    TYPE_OF,             // Push type name
    IS_INSTANCE_OF,      // Check if instance of class name (string on stack)
    
    // Async
    OP_AWAIT,            // Wait for Future to resolve
    
    // Models / OOP
    MAKE_INTERFACE,      // name_idx, method_count: Create an interface
    MAKE_CLASS,          // name_idx, method_count, interface_count: Create a class
    
    // Debugging
    BREAKPOINT,          // Debugger breakpoint
    LINE,                // line_num: Source line info
    HAS_GLOBAL,          // name_idx: Push true if global exists

    // Fast global slot access (Issue C optimization)
    LOAD_GLOBAL_SLOT,    // slot_idx (uint16): globalSlots[slot] -> push
    STORE_GLOBAL_SLOT,   // slot_idx (uint16): peek -> globalSlots[slot]
    
    END = 255            // End of chunk marker
};

// Instruction encoding helpers
inline uint8_t opcodeByte(OpCode op) { return static_cast<uint8_t>(op); }

// ============================================================================
// Constant Pool Entry
// ============================================================================
struct Constant {
    enum class Type {
        NIL,
        BOOL,
        INT,
        DOUBLE,
        STRING,
        FUNCTION,
        MODEL,
        ARRAY_CONST  // For array literals
    };
    
    Type type;
    std::variant<
        std::nullptr_t,
        bool,
        long long,
        double,
        std::string,
        struct FunctionConstant*,
        struct ModelConstant*,
        std::vector<size_t>  // For array constant indices
    > value;
    
    // Constructors
    Constant() : type(Type::NIL), value(nullptr) {}
    explicit Constant(bool b) : type(Type::BOOL), value(b) {}
    explicit Constant(long long i) : type(Type::INT), value(i) {}
    explicit Constant(double d) : type(Type::DOUBLE), value(d) {}
    explicit Constant(const std::string& s) : type(Type::STRING), value(s) {}
    explicit Constant(FunctionConstant* f);
    explicit Constant(ModelConstant* m);
};

// Function constant (stored in constant pool)
struct FunctionConstant {
    std::string name;
    size_t arity;
    bool isVariadic;
    bool isAsync;
    size_t upvalueCount;
    std::vector<std::string> paramNames;
    std::vector<Constant> defaultValues; // NIL = no default
    
    // Bytecode location (offset in chunk)
    size_t codeOffset;
    size_t codeLength;
    
    // Local variable info for debugging
    size_t localCount;
};

// Model constant
struct ModelConstant {
    std::string name;
    std::string parent;  // Empty if none
    std::vector<std::string> interfaces;
    
    struct Member {
        std::string name;
        bool isMethod;
        bool isStatic;
        size_t valueIdx;  // Constant index for method/initial value
    };
    std::vector<Member> members;
    
    // Init function index
    size_t initFunctionIdx;
};

// ============================================================================
// Bytecode Chunk
// ============================================================================
struct Chunk {
    std::vector<uint8_t> code;           // Bytecode instructions
    std::vector<Constant> constants;     // Constant pool
    std::vector<Value>    resolvedConstants; // Cached Value versions of constants
    std::vector<size_t> lines;           // Source line for each bytecode
    
    // Helper methods
    size_t addConstant(const Constant& constant);
    size_t writeByte(uint8_t byte, size_t line);
    void writeOp(OpCode op, size_t line);
    void writeBytes(uint8_t b1, uint8_t b2, size_t line);
    void writeBytes(uint8_t b1, uint8_t b2, uint8_t b3, size_t line);
    void writeJump(OpCode op, size_t line);
    void patchJump(size_t offset);
    void writeLoop(size_t loopStart, size_t line);
    
    // Get constant
    const Constant& getConstant(size_t idx) const;
    void resolveConstants();
    
    // Disassembly
    void disassemble(const std::string& name) const;
    size_t disassembleInstruction(size_t offset) const;
};

// ============================================================================
// Local Variable (for compiler)
// ============================================================================
struct Local {
    std::string name;
    size_t depth;        // Scope depth
    bool isCaptured;     // Closed over by closure
    bool isConst;        // Const variable
    bool isStackResident; // True if it occupies a slot on the execution stack (needs POP)
    bool exported;       // Visible when module is imported via namespaced use
    
    // For debug tracking
    size_t startPC;
    size_t localVarInfoIdx;
};

// ============================================================================
// Upvalue (for closures)
// ============================================================================
struct Upvalue {
    enum class Type { LOCAL, UPVALUE };
    Type type;
    size_t index;        // Local slot or upvalue index
};

// ============================================================================
// Local Variable Debug Info (runtime tracking)
// ============================================================================
struct LocalVarInfo {
    std::string name;
    size_t slot;
    size_t startPC;
    size_t endPC;
};

// ============================================================================
// Bytecode Function (runtime representation)
// ============================================================================
struct BytecodeFunction {
    std::string name;
    std::string filename;  // Source file this function was defined in
    size_t arity;
    bool isVariadic;
    bool isAsync;
    bool isMethod;
    Chunk chunk;
    std::vector<Upvalue> upvalues;
    size_t upvalueCount;
    size_t localCount;
    
    // Debug info for crash dumps
    std::vector<LocalVarInfo> localVars;
    
    // Nested compiled functions referenced by CLOSURE opcodes.
    // CLOSURE operand N → nestedFunctions[N].
    std::vector<std::shared_ptr<BytecodeFunction>> nestedFunctions;

    // Number of parameters that have default values (rightmost N params).
    // Used by the VM to compute minArity = arity - defaultParamCount.
    size_t defaultParamCount;
    size_t globalSlotCount;  // Number of global slots used (for LOAD/STORE_GLOBAL_SLOT)

    BytecodeFunction(const std::string& name, size_t arity)
        : name(name), filename(""), arity(arity), isVariadic(false), isAsync(false), isMethod(false),
          upvalueCount(0), localCount(0), defaultParamCount(0), globalSlotCount(0) {}
};

using BytecodeFunctionPtr = std::shared_ptr<BytecodeFunction>;

// ============================================================================
// Compilation Result
// ============================================================================
struct CompileResult {
    bool success;
    std::string error;
    BytecodeFunctionPtr mainFunction;  // Entry point
    std::vector<BytecodeFunctionPtr> functions;  // All functions
    // Global slot table: index -> name (for VM initialization)
    std::vector<std::string> globalSlotNames;
    
    CompileResult() : success(false) {}
};
#endif // BYTECODE_H
