# 🏛️ Architecture Overview & Engine Design

This document details the high-level architecture, module breakdown, execution pipeline, and threading model of the **EZ Programming Language Interpreter**.

---

## 1. 🔄 Complete Execution Pipeline

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────────────┐
│  Source Code │ ──> │    Lexer     │ ──> │    Parser    │ ──> │ Abstract Syntax Tree │
│    (.ez)     │     │  (Tokens)    │     │  (Recursive) │     │        (AST)         │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────┬───────────┘
                                                                          │
                                                                          ▼
┌──────────────┐     ┌──────────────┐     ┌──────────────┐     ┌──────────────────────┐
│ Native OS /  │ <── │ Virtual Mach.│ <── │   Bytecode   │ <── │  Bytecode Compiler   │
│ FFI / Builtin│     │ (Execute VM) │     │    Chunks    │     │  (Optimizations)     │
└──────────────┘     └──────────────┘     └──────────────┘     └──────────────────────┘
```

1. **Lexical Analysis (`src/lexer/`)**: Tokenizes source text into a stream of typed `Token` structures, preserving file names, line numbers, and column offsets.
2. **Parsing & AST Construction (`src/parser/`, `src/ast/`)**: Builds a strongly-typed Abstract Syntax Tree with high-precision source location metadata.
3. **Bytecode Compilation (`src/compiler/`)**: Translates AST nodes into sequential bytecode `Chunk`s. Performs compile-time AST constant folding, dead code pruning, algebraic simplifications, and superinstruction emissions.
4. **Bytecode Virtual Machine (`src/vm/`)**: Executes bytecode using a high-performance **computed-goto dispatch loop**, direct method inline caching (IC), and a flat call-frame stack.
5. **Runtime & Concurrency Engine (`src/runtime/`, `src/gc/`, `src/eventloop/`)**: Manages tagged `Value` variants, Bacon-Rajan cycle-collecting GC, libuv async event loop, and multithreaded task workers.

---

## 2. 📁 Source Tree Directory Map

```
src/
├── ast/              # AST structure definitions (Expr, Stmt, TypeAST)
├── bytecode/         # OpCode enum definitions, Chunk serialization, Disassembler
├── builtins/         # Native C++ builtin functions (IO, Net, Math, String, Time)
│   └── FFI/          # Native C FFI (Dynamic loading, safe memory, callbacks)
├── cli/              # Command-line entry point, REPL, Packager (.exe compiler)
├── compiler/         # Bytecode compiler (AST -> Bytecode, Optimizations)
├── eventloop/        # libuv integration and async task queue
├── gc/               # Bacon & Rajan cycle-detecting garbage collector
├── gui/              # Native Win32 GUI subsystem
├── lexer/            # Scanner, Tokenizer, Source coordinate tracking
├── parser/           # Recursive-descent & Pratt parser
├── runtime/          # Value variant, Environment, Objects, Futures
├── typechecker/      # Static type inference and validation
├── utils/            # Arithmetic wrappers, JSON helpers, Path resolution
└── vm/               # BytecodeVM execution loop, Inline Caches, Method Dispatch
```

---

## 3. 🧵 Threading & Concurrency Model

```
                    ┌────────────────────────────┐
                    │   Main Thread (EventLoop)  │
                    │   • libuv Async I/O        │
                    │   • Main BytecodeVM        │
                    │   • Global Environment     │
                    └─────────────┬──────────────┘
                                  │ spawn(task)
             ┌────────────────────┼────────────────────┐
             ▼                                         ▼
┌─────────────────────────┐               ┌─────────────────────────┐
│     Worker Thread 1     │               │     Worker Thread 2     │
│   • Isolated BytecodeVM │               │   • Isolated BytecodeVM │
│   • Dedicated Op Stack  │               │   • Dedicated Op Stack  │
│   • Cloned Call Stack   │               │   • Cloned Call Stack   │
└─────────────────────────┘               └─────────────────────────┘
```

- **Thread Isolation**: Each worker thread gets its own `BytecodeVM` instance and evaluation stack. Execution inside `BytecodeVM::run()` is single-threaded and lock-free for maximum speed.
- **Shared Global Environment**: Globals, classes, and immutable constants are shared across threads with atomic operations or reader-writer synchronizations.
- **Upvalue Closure Invariant**: Before a thread is spawned via `spawn()`, all open lexical closures in parent frames are **recursively closed** (`closeUpvalues()`) to prevent dangling stack pointers.
- **Async I/O Event Loop**: The main thread runs a `libuv` event loop singleton (`EventLoop::instance()`) that coordinates cross-thread messages via thread-safe task queues.
