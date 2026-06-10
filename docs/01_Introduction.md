# EZ Language - Introduction

Welcome to EZ! EZ is a powerful, expressive, and dynamically typed scripting language designed for both quick scripting and full-scale application development. It features a clean syntax inspired by modern languages, a custom bytecode virtual machine for performance, and a comprehensive standard library.

## Getting Started

A basic "Hello, World!" program in EZ:

```ez
out "Hello, World!"
```

## Core Philosophies
1. **Readability**: EZ uses keywords like `when`, `other`, `repeat`, and `task` to make code read like natural language.
2. **Safety & Concurrency**: With built-in `spawn`, `await`, and `try/catch`, handling complex async flows and errors is seamless.
3. **Systems Interop**: C-compatible `struct` support and powerful FFI via `os_call` means you can directly interface with native DLLs and system libraries.
