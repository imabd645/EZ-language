# 📚 EZ Language Documentation Index

Welcome to the official EZ Language Documentation. The documentation is organized into two primary tracks:

1. **[User Guides & Language Manual (`docs/user/`)](#-user-guides--language-manual)**: For programmers writing applications, scripts, and libraries in EZ.
2. **[Engine Internals & Architecture (`docs/internals/`)](#-engine-internals--compiler-architecture)**: For C++ engineers developing, maintaining, and optimizing the EZ language compiler, runtime, and Virtual Machine.

---

## 📖 User Guides & Language Manual

| Guide | Description |
| :--- | :--- |
| **[01. Getting Started](user/01_Getting_Started.md)** | Installation, running scripts, CLI flags, REPL, and tools. |
| **[02. Syntax & Data Types](user/02_Syntax_and_Variables.md)** | Variables, scoping rules, primitives, strings, arrays, dictionaries, and tuples. |
| **[03. Control Flow](user/03_Control_Flow.md)** | `when`/`other` branching, `while` and `repeat` loops, pattern matching. |
| **[04. Functions & Closures](user/04_Functions_and_Tasks.md)** | `task` declarations, default values, variadics, closures, lambdas, and type annotations. |
| **[05. Object-Oriented Programming](user/05_Object_Oriented_Programming.md)** | Classes, constructors, inheritance, `self`, `super`, interfaces, and structs. |
| **[06. Concurrency & Async](user/06_Concurrency_and_Async.md)** | Asynchronous tasks, `spawn()`, `await()`, futures, channels, and atomics. |
| **[07. Standard Library & Packages](user/07_Standard_Library.md)** | Core built-in functions and the `ezlib` ecosystem. |
| **[08. Native C / Win32 FFI](user/08_Native_FFI.md)** | Calling C DLLs, pointer manipulation, native memory buffers, and callbacks. |
| **[09. Native GUI Development](user/09_GUI_Development.md)** | Building modern Windows desktop applications with native GUI controls. |

---

## ⚙️ Engine Internals & Compiler Architecture

| Architecture Document | Focus Area |
| :--- | :--- |
| **[01. Architecture Overview](internals/01_Architecture_Overview.md)** | System pipeline, threading model, execution flow, and source tree map. |
| **[02. Lexer, Parser & AST](internals/02_Lexer_Parser_AST.md)** | Tokenization, Pratt parser, AST node hierarchies, and error diagnostic tracking. |
| **[03. Compiler & Bytecode](internals/03_Compiler_and_Bytecode.md)** | AST-to-Bytecode compiler, slot resolution, constant folding, and superinstructions. |
| **[04. Virtual Machine Execution Engine](internals/04_Virtual_Machine.md)** | Computed goto dispatch, call frame stack, inline caching (IC), and method invocations. |
| **[05. Value System & Objects](internals/05_Value_System.md)** | Tagged `Value` variant, object layouts, closures, bound methods, short-string optimization. |
| **[06. Cycle-Collecting Garbage Collector](internals/06_Garbage_Collection.md)** | Bacon & Rajan reference count cycle detection, trial deletion, and memory thresholds. |
| **[07. Concurrency & Libuv Event Loop](internals/07_Concurrency_and_EventLoop.md)** | libuv integration, cross-thread message passing, worker VMs, and upvalue safety. |
| **[08. FFI Subsystem & Memory Safety](internals/08_FFI_Subsystem.md)** | Dynamic loading, Windows SEH / vectored exception handlers, and safe memory macros. |
| **[09. Exception Handling Engine](internals/09_Exception_Handling.md)** | Frame-unwinding pipeline, `TryBlock` stack, and zero-cost exception tables. |
