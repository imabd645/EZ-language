# AGENTS.md - Guidelines for AI Coding Agents

## Build, Test, and Run Commands
## Never compile the interpreter Yourself
### Building the Project

**Primary Build (MinGW g++):**
```bash
build.bat
```
- Uses `C:\msys64\mingw64\bin\g++.exe`
- Produces `ez.exe` in current directory
- Requires all DLLs in `dlls/` directory to be present

**Alternative Build (CMake):**
```bash
build_cmake.bat
```
- Creates `build/` directory
- Configures with CMake and builds with 8 parallel jobs
- Produces `build\ez.exe`

### Running EZ Programs

**Execute script:**
```bash
ez.exe script.ez
```

**REPL mode:**
```bash
ez.exe
```

**Trace execution (debug):**
```bash
ez.exe -trace script.ez
```

### Testing

**Run test suite:**
```bash
ez.exe Test/test.ez
```

**Run specific test:**
```bash
ez.exe Test/test_buffer.ez
```

**Run handwritten tests:**
```bash
ez.exe Test/HandwrittenTests/arr_51_empty_array.ez
```

## Directory Map

```
src/
├── ast/              # Abstract Syntax Tree definitions
├── bytecode/         # Bytecode representation and serialization
├── builtins/         # Native function implementations
├── cli/              # Command-line interface, REPL, packager
├── compiler/         # AST to bytecode compiler
├── eventloop/        # libuv-based async I/O event loop
├── gc/               # Cycle-detecting garbage collector
├── gui/              # Windows GUI builtins
├── lexer/            # Tokenizer
├── parser/           # Recursive descent parser
├── runtime/          # Value system, Environment, objects
├── typechecker/      # Static type checking
├── utils/            # Utility functions (MiniJson)
└── vm/               # Bytecode virtual machine

Test/                 # Test files (.ez)
examples/             # Example programs (.ez)
docs/                 # Existing documentation
dlls/                 # Windows DLL dependencies
lib/                  # EZ library modules (ezlib)
```

## Code Conventions

### Naming

**C++ Files:**
- Headers: PascalCase (e.g., `BytecodeVM.h`, `RuntimeContext.h`)
- Source: PascalCase (e.g., `BytecodeVM.cpp`, `Builtins.cpp`)
- Classes: PascalCase (e.g., `BytecodeVM`, `EZClass`, `Value`)
- Methods: camelCase (e.g., `defineGlobal`, `runtimeError`)
- Member variables: camelCase (e.g., `stackTop`, `globalEnv`)

**EZ Language:**
- Variables: camelCase
- Functions: camelCase
- Classes: PascalCase
- Constants: UPPER_SNAKE_CASE (convention, not enforced)

### Error Handling

**Runtime Errors:**
- Use `interp.runtimeError(message, line, filename)` for recoverable errors
- Use `interp.throwException(className, message, line, filename)` for typed exceptions
- Always provide line numbers and filename when available

**Type Checking:**
- Check argument types before use: `args[0].isString()`, `args[1].isNumber()`
- Return early with error if types don't match
- Use specific exception types: TypeError, ValueError, IndexError, etc.

### Builtin Registration Pattern

```cpp
interp.defineGlobal("functionName", Value::makeNativeFunction("functionName", arity,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        // Type checking
        if (!args[0].isString()) {
            interp.runtimeError("functionName() expects string as first argument", 0, "");
            return Value();
        }
        
        // Implementation
        std::string s = args[0].asString();
        // ... do work ...
        
        // Return value
        return Value(result);
    }));
```

**Arity:**
- Use exact number for fixed arity: `1`, `2`, `3`
- Use `-1` for variadic functions
- Check `args.size()` for variadic validation

## Hard Rules (Invariants)

### VM Execution Threading

**Rule**: VM execution must only run on its owning thread.
- Each `BytecodeVM` instance is not thread-safe
- Cross-thread calls must marshal through `EventLoop::instance().pushTask()`
- Spawned threads get isolated VM instances

**Enforcement**: 
- `BytecodeVM::run()` assumes single-threaded execution
- No mutex protection inside VM execution loop
- Global state (Environment) is shared but VM-local state is not

### FFI Memory Safety

**Rule**: All FFI memory operations must use `SAFE_MEMORY_OP` macro on Windows.
- Prevents access violations from crashing the interpreter
- Uses SEH (MSVC) or vectored exception handlers (MinGW)

**Example**:
```cpp
SAFE_MEMORY_OP(interp, val = *(uint64_t*)(base + offset));
```

### Upvalue Closure

**Rule**: Upvalues must be closed before spawning threads.
- `spawn()` closes upvalues to avoid dangling stack pointers
- Nested closures must be recursively closed
- Arrays, Dicts, Instances are shared directly (shared_ptr)

**Implementation**: See `Builtins_GC.cpp::spawn()` for closure logic.

### Global Environment Sharing

**Rule**: Global Environment is shared across all VM instances.
- Modifications affect all threads
- Use mutex for concurrent access if needed
- Global slots are pre-initialized from compiler output

## Common Pitfalls

### Platform-Specific Code

**Windows Macros**: libuv and Windows headers conflict with EZ tokens.
- Must undefine `INTERFACE`, `ERROR`, `IN`, `OUT`, `TRUE`, `FALSE` before including libuv
- See `src/eventloop/EventLoop.h` for pattern

**SEH vs setjmp**: Different exception handling for MSVC vs MinGW.
- MSVC: `__try` / `__except`
- MinGW: `setjmp` / `longjmp` with vectored exception handler
- See `src/builtins/FFI/` for pattern (split by concern: `FFI_Memory.cpp`, `FFI_Call.cpp`, `FFI_Callback.cpp`, `FFI_Struct.cpp`; shared plumbing in `FFI_Internal.h`)

**Hardcoded Paths**: Ezlib path is platform-specific.
- Windows: `C:/ezlib`
- Unix: `/usr/local/lib/ezlib`
- See `src/compiler/BytecodeCompilerStmt.cpp`

### Memory Management

**Cycle Collection**: Objects with cycles must be tracked.
- Call `CycleCollector::instance().track(ptr, type)` on construction
- Only needed for container objects (arrays, dicts, instances, classes)
- Primitives and simple values don't need tracking

**String Concatenation**: Use lazy concatenation for performance.
- `EZConcatString` for building large strings
- `bytesToString()` for byte array to string conversion
- Avoid manual concatenation in loops

### Concurrency

**Future Waiting**: Always check if future is ready before `get()`.
- Use `fut->wait()` to block until ready
- `awaitAll()` and `awaitAny()` for multiple futures
- Don't call `get()` on unwaited futures

**Mutex Deadlocks**: Always use RAII locking.
- `lock(mutex, lambda)` ensures unlock even on exception
- Don't manually lock/unlock in EZ code
- See `Builtins_Concurrency.cpp::lock()`

## Adding a New Native Builtin

### Step-by-Step Example

**Goal**: Add a `reverseString()` builtin that reverses a string.

**1. Choose the appropriate file:**
- String operations → `Builtins_String.cpp`
- Math operations → `Builtins_Math.cpp`
- New subsystem → Create new `Builtins_Subsystem.cpp`

**2. Add the registration in the appropriate register function:**

```cpp
// In Builtins_String.cpp, inside registerStringBuiltins()

interp.defineGlobal("reverseString", Value::makeNativeFunction("reverseString", 1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        // Type check
        if (!args[0].isString()) {
            interp.runtimeError("reverseString() expects string", 0, "");
            return Value();
        }
        
        // Get string
        std::string s = args[0].asString();
        
        // Reverse
        std::reverse(s.begin(), s.end());
        
        // Return
        return Value(s);
    }));
```

**3. Register the subsystem in main builtin registration:**

```cpp
// In Builtins.cpp, inside registerBuiltins()

void registerBuiltins(RuntimeContext& interp) {
    registerIOBuiltins(interp);
    registerNetBuiltins(interp);
    registerMathBuiltins(interp);
    registerStringBuiltins(interp);  // Already here if using existing file
    // ...
}
```

**4. Add to BUILTINS.md:**
- Document the function signature
- Add example usage
- Mark confidence tier (Confirmed/Inferred)

**5. Test:**
```ez
// test_reverse.ez
out reverseString("hello")  // Should print "olleh"
```

**6. Run test:**
```bash
ez.exe test_reverse.ez
```

### Variadic Functions

For functions with variable argument counts:

```cpp
interp.defineGlobal("format", Value::makeNativeFunction("format", -1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        if (args.empty()) {
            interp.runtimeError("format() expects at least 1 argument", 0, "");
            return Value();
        }
        
        std::string format = args[0].asString();
        // Process remaining args...
        return Value(result);
    }));
```

### Class-Based Builtins

For classes like `Atomic` or `Channel`:

```cpp
// Define class
auto myClass = std::make_shared<EZClass>("MyClass");
CycleCollector::instance().track(myClass, ValueType::CLASS);

// Add methods
myClass->setMethod("init", Value::makeNativeFunction("init", 1,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        auto instance = args[0].asInstance();
        // Initialize instance properties
        instance->setProperty("_internal", Value(/* ... */));
        return args[0];
    }));

myClass->setMethod("doSomething", Value::makeNativeFunction("doSomething", 0,
    [](RuntimeContext& interp, const std::vector<Value>& args) -> Value {
        auto instance = args[0].asInstance();
        // Access instance properties
        Value internal = instance->getProperty("_internal");
        // Do work...
        return Value(result);
    }));

// Register globally
interp.defineGlobal("MyClass", Value(myClass));
```

## Mandatory Syntax Rule for Test Files

**CRITICAL**: When writing new `.ez` test files, you MUST use the exact call syntax recorded in `BUILTINS.md` for every builtin you call.

**Rules:**
1. **Tier 1 (Confirmed) examples take precedence** - Use the exact syntax from real `.ez` files found in the codebase
2. **Tier 2 (Inferred) examples** - If no confirmed example exists, use the inferred syntax from C++ registration
3. **Do not improvise** - Never guess argument order, types, or return value handling
4. **If builtin not documented** - Look up the C++ registration directly, add a correctly-tiered entry to `BUILTINS.md` first, then write the test

**Example:**
```ez
// WRONG - guessing argument order
result = someBuiltin(value1, value2)  // Don't do this!

// RIGHT - using documented syntax
result = someBuiltin(value2, value1)  // As documented in BUILTINS.md
```

**Process for undocumented builtins:**
1. Find the C++ registration in `src/builtins/`
2. Extract arity, parameter types, and return type
3. Add entry to `BUILTINS.md` with Tier: Inferred
4. Write test using that syntax
5. If test passes, update Tier to Confirmed

## Testing Expectations

### Running the Test Suite

**Main test file:**
```bash
ez.exe Test/test.ez
```

**Specific category tests:**
- Buffer: `Test/test_buffer.ez`
- Async: `Test/test_async_v2.ez`
- Dict: `Test/test_dict.ez`
- Concurrency: `Test/test_concurrency.ez`

**Handwritten tests:**
```bash
ez.exe Test/HandwrittenTests/arr_51_empty_array.ez
```

### Test Organization

**Test/ directory structure:**
- Root: Integration tests and feature demos
- `HandwrittenTests/`: Minimal unit tests for specific features
- Naming convention: `category_number_description.ez`

**What "passing" means:**
- Script executes without runtime errors
- Expected output matches (visual inspection)
- No crashes or access violations
- For async tests: futures resolve correctly

### Adding New Tests

**Location:**
- Feature tests: `Test/`
- Unit tests: `Test/HandwrittenTests/`
- Examples: `examples/`

**Naming:**
- Use descriptive names: `test_new_feature.ez`
- For handwritten: `category_XX_description.ez` (increment XX)

**Structure:**
```ez
// test_new_feature.ez
out "=== Testing New Feature ==="

// Test case 1
result = newBuiltin(arg1, arg2)
out "Result: " + str(result)

// Test case 2
try {
    newBuiltin(invalid_arg)
    out "FAIL: Should have thrown error"
} catch e {
    out "PASS: Correctly threw error"
}

out "=== Tests Complete ==="
```

## Existing Documentation

**Current docs in `docs/`:**
- `01_Introduction.md` - Language overview
- `02_Variables_and_Data_Types.md` - Type system
- `03_Control_Flow.md` - Control structures
- `04_Functions_and_Tasks.md` - Functions and async
- `05_Object_Oriented_Programming.md` - OOP features
- `06_Interfaces_and_Structs.md` - Interfaces and structs
- `07_Error_Handling.md` - Exception handling
- `08_Modules_and_Concurrency.md` - Modules and threading
- `09_Builtin_Libraries.md` - Standard library overview
- `api.md` - API reference
- `faq.md` - Frequently asked questions
- `gui_readme.md` - GUI documentation
- `specification.md` - Language specification
- `tutorial.md` - Tutorial

**When updating docs:**
- Keep existing structure
- Add new sections as needed
- Cross-reference ARCHITECTECTURE.md for implementation details
- Update BUILTINS.md when adding new functions
