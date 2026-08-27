# ⚡ Virtual Machine Execution Engine

This document details the architecture and execution mechanics of the **BytecodeVM** (`src/vm/`).

---

## 1. 🏎️ Computed Goto Dispatch Engine

On GCC and MinGW Clang (`#if defined(__GNUC__)`), the VM uses **Computed Goto (Direct Threaded Code)** instead of a giant `switch` statement:

```cpp
// Static jump table of instruction label pointers
static void* dispatchTable[] = {
    &&handle_LOAD_CONST,
    &&handle_LOAD_LOCAL,
    &&handle_ADD,
    &&handle_INVOKE_METHOD,
    ...
};

#define DISPATCH() goto *dispatchTable[READ_BYTE()]
```

### Benefits:
- Eliminates indirect jump branch mispredictions caused by a shared `switch` jump table.
- Each opcode directly branches to the next opcode's target address, matching native C interpreter speeds (LuaJIT / CPython 3.12+ style).

---

## 2. 🗄️ Call Frame & Operand Stack Layout

```
Stack Top ➔  [ Operand Slot N    ]  <- stackTop (evaluations, temps)
             [ Operand Slot N-1  ]
             ├───────────────────┤
             [ Local Slot 2      ]
             [ Local Slot 1      ]
Frame Base ➔ [ Local Slot 0 (self)] <- frame->slots
             ├───────────────────┤
             [ Callee / Result   ]  <- frame->slots - 1
```

- **Call Stack (`std::vector<CallFrame>`)**: Holds active function contexts. Each `CallFrame` stores its instruction pointer `ip`, its base slot pointer `slots`, and reference to the executing `EZFunction`.
- **Operand Stack (`Value stack[STACK_MAX]`)**: Flat array of 256KB evaluated values.
- **Frame Return Invariant**: When a function returns via `RETURN`, it overwrites `*(frame->slots - 1)` with the return value, collapses the stack top, and restores the parent frame's `ip`.

---

## 3. ⚡ Inline Caching (IC) & Direct Method Dispatch

### Monomorphic Property Cache
When accessing properties (`obj.x` via `LOAD_PROPERTY` / `STORE_PROPERTY`), the VM caches the receiver's `EZClass*` and the property's index offset in a thread-local `ICCacheEntry`:

```cpp
if (ic->cachedClass == instance->klass) {
    // Fast path: Direct slot offset read without map lookup or mutex locking
    *stackTop++ = instance->fields[ic->cachedOffset];
    DISPATCH();
}
```

### Direct Method Invocation (`INVOKE_METHOD`)
Instead of performing a property load (`LOAD_PROPERTY` $\rightarrow$ pushes BoundMethod) followed by a generic function call (`CALL`), the compiler emits `INVOKE_METHOD`:
1. Looks up the method directly in the class method table or cached slot.
2. Places the receiver `obj` into `slot 0` (`self`).
3. Dispatches the function call frame directly with **zero intermediate `BoundMethod` object allocations on the heap**.
