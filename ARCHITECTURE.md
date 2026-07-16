# EZ Language Architecture

## Overview

EZ is a high-level, dynamically-typed programming language with a stack-based bytecode VM, built-in concurrency support, and native FFI capabilities. It compiles source code to bytecode which is executed by a virtual machine with cycle-detecting garbage collection.

## Execution Pipeline

```
Source (.ez) → Lexer → Parser → AST → Bytecode Compiler → Bytecode → VM Execution → Runtime/Builtins/FFI
```

1. **Lexer** (`src/lexer/Lexer.cpp`): Tokenizes source code into tokens
2. **Parser** (`src/parser/Parser.cpp`): Generates Abstract Syntax Tree (AST) from tokens
3. **Bytecode Compiler** (`src/compiler/BytecodeCompiler.cpp`): Compiles AST to bytecode chunks
4. **VM** (`src/vm/BytecodeVM_Execute.cpp`): Stack-based interpreter executing bytecode
5. **Runtime** (`src/runtime/RuntimeContext.h`): Central interface for execution context, error handling, and global state

## Module Breakdown

### Core VM (`src/vm/`)

**BytecodeVM.h/cpp** - Main virtual machine implementation
- Stack-based execution with configurable stack size (default 256KB)
- Call frames with up to 512 nested calls
- Computed goto dispatch (GCC) or switch statement (MSVC)
- Exception handling with try/catch/finally blocks
- Upvalue management for closures

**BytecodeVM_Execute.cpp** - Execution loop
- 70+ opcodes including: LOAD_CONST, CALL, RETURN, INDEX_GET, MAKE_ARRAY, etc.
- Hot-path optimizations for arithmetic and comparisons
- Thread-local string interning pool

**BytecodeVM_Objects.cpp** - Object instantiation and method dispatch
- Class instantiation with inheritance
- Method binding and super calls
- Property access with visibility controls

### Value System (`src/runtime/Value.h`)

The `Value` type is a `std::variant` supporting 23 distinct types:

- **Primitives**: NIL, BOOL, NUMBER (double), INTEGER (long long)
- **Strings**: STRING (std::shared_ptr<std::string>), SHORT_STRING (14-byte inline), CONCAT_STRING (lazy concatenation)
- **Collections**: ARRAY, DICTIONARY, TUPLE
- **Functions**: FUNCTION (EZ bytecode), NATIVE_FUNCTION (C++ lambda), CLOSURE_VAL, BOUND_METHOD
- **OOP**: CLASS, INSTANCE, INTERFACE, SUPER
- **Concurrency**: FUTURE, MUTEX, ATOMIC, CHANNEL
- **FFI**: BUFFER

**Type checking**: O(1) via index lookup table mapping variant index to enum
**String optimization**: Short strings (≤14 chars) stored inline, longer strings use shared_ptr
**Concatenation**: Lazy EZConcatString for efficient string building

### Memory Management (`src/gc/CycleCollector.h`)

**Algorithm**: Bacon & Rajan (2001) reference-count adjustment
- No root-set scanning, no Stop-The-World, no safepoints
- Works alongside `shared_ptr` refcounting
- Detects and breaks reference cycles that would otherwise leak

**Phases**:
1. Purge expired weak_ptrs (naturally freed objects)
2. Build candidate snapshot with adjusted reference counts
3. Flood-fill from externally-reachable candidates (adjustedRC > 0)
4. Break outgoing references on garbage objects

**Configuration**:
- Minor threshold: 2000 tracked objects
- Major threshold: 10000 tracked objects
- Can be disabled/enabled via `gc_disable()`/`gc_enable()`

### Concurrency Model (`src/eventloop/EventLoop.h`, `src/runtime/EZFuture.h`)

**Event Loop**: libuv-based singleton for async I/O
- Cross-thread task queuing via `pushTask()`
- Reference counting to prevent premature exit
- Integrates with libuv timers and async handles

**Futures**: Windows Event-based synchronization
- `EZFuture` wraps Windows `HANDLE` (Event)
- Supports `wait()`, `get()`, `cancel()`
- Used by `spawn()`, `fetch()`, `waitAsync()`

**Threading**:
- `spawn()` creates native C++ threads with isolated VM instances
- Upvalue closure before thread spawn to avoid dangling stack pointers
- Shared `Environment` for global state across threads
- Atomic operations via `std::atomic` for `Atomic` class

**Primitives**:
- `mutex()` / `lock()` - RAII mutex locking
- `wait()` - Synchronous sleep
- `waitAsync()` - Asynchronous timer via libuv
- `Atomic` class - Atomic integer operations
- `Channel` class - Thread-safe queue with condition variables

### FFI Layer (`src/builtins/Builtins_FFI.cpp`)

**libffi Integration**:
- Dynamic foreign function calls with type signatures
- Support for int, float, double, string, pointer types
- Callback registration from EZ to native code

**Windows-Specific Features**:
- Structured Exception Handling (SEH) for crash protection
- Vectored exception handlers for non-MSVC compilers
- Memory access validation via `SAFE_MEMORY_OP` macro
- Proxy window procedure for GUI interop

**Memory Operations**:
- `os_alloc()` / `os_free()` - Native heap allocation
- `os_read_*` / `os_write_*` - Typed memory access (int8-64, float32-64)
- `os_buffer_from_ptr()` / `os_buffer_addr()` - Buffer/pointer conversion

**DLL Loading**:
- `os_load_lib()` - LoadLibraryA wrapper
- `os_get_func()` - GetProcAddress wrapper
- `os_call()` / `os_call_sig()` - Function invocation

### Parser & Compiler

**Parser** (`src/parser/Parser.cpp`): Recursive descent with precedence climbing
- Supports: if/when, while/repeat, for-each, try/catch, async/task
- OOP: classes, interfaces, structs, inheritance, super calls
- Functions: lambdas, closures, variadic functions, default parameters
- Modules: `use` statements with ezlib path resolution

**Compiler** (`src/compiler/BytecodeCompiler.cpp`): AST to bytecode
- Constant pooling
- Global slot optimization (Issue C)
- Closure upvalue tracking
- Tail call optimization (`TAIL_CALL` opcode)

### Builtin Subsystems

**IO** (`src/builtins/Builtins_IO.cpp`): File I/O and console
- `print()`, `input()`, `readFile()`, `writeFile()`, `readLines()`

**Math** (`src/builtins/Builtins_Math.cpp`): Basic math functions
- `floor()`, `ceil()`, `abs()`, `sqrt()`, `pow()`, `rand()`, `randint()`, `round()`, `min()`, `max()`

**String** (`src/builtins/Builtins_String.cpp`): String manipulation
- `substr()`, `split()`, `join()`, `upper()`, `lower()`, `trim()`, `replace()`
- Regex: `reMatch()`, `reSearch()`, `reReplace()`
- Encoding: `hex_to_bytes()`, `b64url_encode()`, `b64url_decode()`

**Data** (`src/builtins/Builtins_Data.cpp`): Collection operations
- `len()`, `push()`, `pop()`, `keys()`, `values()`, `map()`, `filter()`, `reduce()`
- JSON: `parse_json()`, `to_json()`

**Concurrency** (`src/builtins/Builtins_Concurrency.cpp`):
- `mutex()`, `lock()`, `wait()`, `waitAsync()`, `spawn()`, `await()`, `cancel()`
- Classes: `Atomic`, `Channel`

**Buffer** (`src/builtins/Builtins_Buffer.cpp`):
- `buffer()`, `buf_size()`, `buf_fill()`, `buf_copy()`, `buf_to_str()`

**GC** (`src/builtins/Builtins_GC.cpp`):
- `gc_disable()`, `gc_enable()`, `gc_collect()`, `gc_set_thresholds()`, `exit()`, `clock()`

**Console** (`src/builtins/Builtins_Console.cpp`): Windows console
- `clear()`, `color()`, `reset()`, `gotoxy()`, `getch()`

**Net** (`src/builtins/Builtins_Net.cpp`): HTTP via libcurl
- `url_encode()`, `url_decode()`, `http_get()`, `http_post()`, `fetch()`

**HTTP** (`src/builtins/Builtins_Http.cpp`): HTTP parsing
- `http_parse_request()`

**Core** (`src/builtins/Builtins_Core.cpp`):
- `panic()`

## Object Model

### Classes and Instances

**EZClass** (`src/runtime/objects/EZClass.h`):
- Method dictionary with visibility flags
- Inheritance via parent pointer
- Shape-based property layout optimization

**EZInstance** (`src/runtime/objects/EZInstance.h`):
- Property dictionary
- Class reference
- Method binding via bound methods

**Interfaces** (`src/runtime/objects/EZInterface.h`):
- Contract-only type checking
- No implementation, only method signatures

### Closures and Upvalues

**EZClosure** (`src/runtime/objects/EZClosure.h`):
- Captures function and upvalue chain
- Used for nested functions and lambdas

**UpvalueObj** (`src/runtime/objects/EZUpvalue.h`):
- Represents a captured variable
- Can be open (pointing to stack) or closed (copied value)
- Linked list for efficient closure management

## Platform Constraints

### Windows-Only Features

The EZ interpreter is currently Windows-only with the following platform-specific dependencies:

1. **Build System**: Requires MinGW g++ or MSVC on Windows
2. **Console Functions**: `color()`, `gotoxy()`, `getch()` use Windows console API
3. **FFI Memory Operations**: SEH-based crash protection only on Windows
4. **Futures**: Windows Event-based synchronization
5. **GUI**: Native Windows GUI integration
6. **Paths**: Hardcoded "C:/ezlib" for ezlib on Windows

### Platform-Specific Code Locations

- `src/builtins/Builtins_FFI.cpp`: Extensive `#ifdef _WIN32` and `#ifdef _MSC_VER`
- `src/builtins/Builtins_Console.cpp`: Windows-only console functions
- `src/builtins/Builtins_Net.cpp`: Winsock2 includes
- `src/eventloop/EventLoop.h`: Windows macro undefs for libuv compatibility
- `src/vm/BytecodeVM.cpp`: Windows console color support
- `src/compiler/BytecodeCompilerStmt.cpp`: Platform-specific ezlib paths

## Build System

### Primary Build: MinGW g++

**Command**: `build.bat`
- Compiler: `C:\msys64\mingw64\bin\g++.exe`
- Optimization: `-O3 -march=native -flto -funroll-loops -fomit-frame-pointer`
- Static linking: `-static -static-libgcc -static-libstdc++`
- Dependencies: libffi, libuv, curl, sqlite3, 25+ Windows DLLs

### Alternative Build: CMake

**Command**: `build_cmake.bat`
- CMake 3.10+ required
- C++17 standard
- Same optimization flags as direct g++ build
- Multi-core compilation via `-j8`

### Dependencies

**Core Libraries**:
- libffi - Foreign function interface
- libuv - Async I/O event loop
- libcurl - HTTP client
- sqlite3 - Database (linked but usage unclear)

**Windows DLLs** (25 in `dlls/` directory):
- OpenSSL: libssl-3-x64.dll, libcrypto-3-x64.dll
- libcurl and dependencies: libcurl-4.dll, libssh2.dll, libnghttp2.dll, etc.
- Compression: libzstd.dll, zlib1.dll, libbz2-1.dll, brotli
- Character encoding: libiconv-2.dll, libunistring-5.dll
- Other: libgcc_s_seh-1.dll, libstdc++-6.dll, libwinpthread-1.dll

## Known Constraints and Tradeoffs

1. **Windows-Only**: No Unix/Linux support due to Windows-specific APIs
2. **Static Linking**: Large executable size (~17MB ez.exe) due to static linking
3. **No JIT**: Interpreted bytecode only, no JIT compilation
4. **Single-Threaded VM**: Each VM instance must run on its owning thread; cross-thread calls marshal through EventLoop
5. **Manual GC**: Cycle collection is automatic but can be disabled; no generational GC
6. **SEH Overhead**: FFI crash protection adds runtime overhead on Windows
7. **Ezlib Path**: Hardcoded paths for library location
8. **No CI/CD**: No automated testing or continuous integration visible in repo

## Threading Model

**VM Thread Affinity**: Each BytecodeVM instance must execute on a single thread
- VM state is not thread-safe
- Cross-thread communication via EventLoop task queuing
- Spawned threads get isolated VM instances with shared global Environment

**Shared State**:
- Global Environment (shared across threads)
- EventLoop (singleton with mutex-protected task queue)
- CycleCollector (singleton with mutex-protected tracking)

**Thread Safety**:
- Arrays, Dicts, Instances use `shared_ptr` and are safe to share
- Mutex class provides explicit synchronization
- Atomic class provides lock-free operations
- Channel class provides thread-safe queue

## Error Handling

**Exceptions**: Try/catch/finally blocks at bytecode level
- `TRY_START`, `TRY_END`, `THROW`, `FINALLY_START`, `FINALLY_END` opcodes
- Exception classes: Exception, FileNotFoundError, NetworkError, TypeError, ValueError, IndexError, KeyError, PermissionError

**Runtime Errors**: Reported via `RuntimeContext::runtimeError()`
- Line numbers and filename tracking
- Source code snippet display on error
- Stack trace printing

**FFI Errors**: Protected by SEH on Windows
- Access violations caught and converted to runtime errors
- Vectored exception handlers for non-MSVC compilers
