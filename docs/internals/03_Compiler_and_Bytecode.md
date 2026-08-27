# ⚙️ Compiler & Bytecode Architecture

This document describes how the EZ AST is compiled into bytecode, including slot resolution, optimization passes, constant folding, and superinstructions.

---

## 1. 📦 Bytecode & Chunk Architecture

Bytecode instructions in EZ are 1-byte opcodes (`OpCode` enum in [`Bytecode.h`](file:///c:/Users/HP/OneDrive/Desktop/EZ/EZ-language/src/bytecode/Bytecode.h)) followed by zero or more operands (e.g. 1-byte local index, 2-byte constant index, 4-byte jump offsets).

A compiled unit of code is encapsulated in a `Chunk`:

```cpp
struct Chunk {
    std::vector<uint8_t>       code;        // Sequential bytecode stream
    std::vector<Constant>      constants;   // Constant pool (literals, strings, functions)
    std::vector<int>           lines;       // Line number mapping per instruction
    std::vector<ICCacheEntry>  icEntries;   // Inline cache slots for property/method dispatch
};
```

---

## 2. 🎯 Variable Slot Resolution (Locals, Upvalues, Globals)

The compiler tracks variable scopes lexically using a single-pass resolver:

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Local Scope Check (resolveLocal)                         │
│    Is variable declared in current function's local array?   │
│    ➔ Yes: Emit LOAD_LOCAL / STORE_LOCAL <slot_index>        │
└──────────────────────────────┬──────────────────────────────┘
                               │ No
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. Upvalue Scope Check (resolveUpvalue)                     │
│    Is variable in an enclosing parent function frame?       │
│    ➔ Yes: Capture Upvalue & emit LOAD_UPVALUE <upvalue_idx> │
└──────────────────────────────┬──────────────────────────────┘
                               │ No
                               ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. Global Slot Check (globalSlots map)                      │
│    Is global slot assigned in the pre-allocated table?      │
│    ➔ Yes: Emit LOAD_GLOBAL_SLOT / STORE_GLOBAL_SLOT <slot>  │
│    ➔ No: Emit slow-path LOAD_GLOBAL <string_name_index>     │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. 🚀 Compiler Optimization Passes

### A. Compile-Time AST Constant Folding
Static expressions are evaluated directly during compilation:
- **String Folding**: `"Hello, " + "World!"` $\rightarrow$ `"Hello, World!"`, `"abc" * 3` $\rightarrow$ `"abcabcabc"`.
- **Bitwise & Modulo**: `1 << 4` $\rightarrow$ `16`, `0xFF & 0x0F` $\rightarrow$ `15`, `100 % 7` $\rightarrow$ `2`.
- **Relational Comparisons**: `10 > 5` $\rightarrow$ `true`, `"a" == "a"` $\rightarrow$ `true`.
- **Pure Builtins**: `len("foo")` $\rightarrow$ `3`, `ord("A")` $\rightarrow$ `65`, `chr(65)` $\rightarrow$ `"A"`, `str(42)` $\rightarrow$ `"42"`.

### B. Dead Code Elimination & Branch Pruning
- `when true { doA() } other { doB() }`: Compiles `doA()` directly with **zero jump instructions**; completely strips `doB()`.
- `when false { doA() } other { doB() }`: Compiles `doB()` directly; completely strips `doA()`.
- `while false { ... }`: Completely pruned with 0 emitted bytes.

### C. Algebraic Identity Reductions
- `x + 0` / `0 + x` $\rightarrow$ `x` (bypasses `ADD`)
- `x - 0` $\rightarrow$ `x` (bypasses `SUB`)
- `x * 1` / `1 * x` $\rightarrow$ `x` (bypasses `MUL`)

### D. Superinstructions
The compiler combines frequent consecutive bytecode pairs into single atomic opcodes:
- `LOAD_LOCAL a` + `LOAD_LOCAL b` + `ADD` $\rightarrow$ `ADD_LOCAL_LOCAL a b`
- `LOAD_LOCAL a` + `LOAD_LOCAL b` + `SUB` $\rightarrow$ `SUB_LOCAL_LOCAL a b`
- `LOAD_GLOBAL_SLOT a` + `LOAD_LOCAL b` + `ADD` $\rightarrow$ `ADD_GLOBAL_LOCAL a b`
- `LOAD_CONST` + `PRINT` $\rightarrow$ `PRINT_STR <idx>`
