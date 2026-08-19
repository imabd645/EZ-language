# Introduction to EZ

## What is EZ?

EZ is a dynamically-typed, bytecode-compiled programming language with a natural,
English-flavoured syntax and an optional static type checker. It is not a toy —
it is a complete language implementation built from scratch in modern C++17,
consisting of:

- a hand-written **lexer** and **recursive-descent parser**,
- an **optional static type checker** that runs between parsing and compilation,
- a **bytecode compiler** that lowers the AST to a custom instruction set,
- a **stack-based virtual machine** with computed-goto dispatch,
- a **cycle-collecting garbage collector** (Bacon & Rajan algorithm) layered on
  top of reference counting,
- a **native FFI** for calling shared-library exports,
- a **Win32 GUI binding**, and
- a package manager / bundler (`ez install`, `ez bundle`) for distributing
  programs as standalone `.exe` files.

EZ replaces the conventional keyword vocabulary of C-family languages with
plain English words: `when` instead of `if`, `task` instead of `function`,
`give` instead of `return`, `other` instead of `else`, `get ... in ...`
instead of `for ... of`. The goal is a language that reads closer to
pseudocode while still compiling to real bytecode and running on a real VM —
not an interpreted toy that walks the AST line by line.

```ez
task greet(name) {
    give "Hello, " + name + "!"
}

out greet("World")        # Hello, World!

people = ["Alice", "Bob", "Carol"]
get person in people {
    out greet(person)
}
```

## Who built it, and why

EZ is authored and maintained by **Abdullah Masood** (GitHub: [imabd645](https://github.com/imabd645)),
a Computer Science student building the language, its VM, and a growing
ecosystem of packages (`ezlib`) as a long-running systems-programming project.
The codebase is roughly 27,000 lines of C++, and the accompanying `ezlib`
standard-library ecosystem (a separate repository, packages installed via
`ez install`) adds roughly 65,000 more lines of EZ-language code across
around 80 packages — everything from ORM and database drivers to an HTTP
server framework, GUI wrappers, and a game loop library.

## Design philosophy

1. **Readable over terse.** Control flow reads like English sentences:
   `when x > 5 { ... } other { ... }`, `get item in list { ... }`,
   `repeat i = 1 to 10 { ... }`.
2. **Dynamic by default, static by choice.** Every example above runs with no
   type annotations at all. When you want compile-time safety, you opt in
   with `:` type annotations on variables, parameters, and return types, and
   EZ's type checker validates them before a single instruction runs.
3. **Batteries partially included.** The C++ runtime ships a substantial set
   of native builtins (string/array/dictionary manipulation, JSON, regex,
   file I/O, HTTP client, concurrency primitives, raw FFI, GUI). Anything
   higher-level — a database ORM, a web server framework, a fluent GUI
   builder — is deliberately built as an EZ-language package on top of those
   primitives, in the external `ezlib` registry, rather than baked into the
   interpreter itself.
4. **Real VM, not a toy interpreter.** EZ compiles to bytecode and executes it
   upvalue semantics, cycle-collecting GC, and a genuine native
   FFI layer capable of calling arbitrary DLL exports and receiving native
   callbacks.
5. **Honesty about scope.** The project's own documentation is candid about
    what is and is not implemented. See the [FAQ](faq.md) and the
    [API reference](api.md) for the current native runtime surface rather than
    assuming feature parity with Python or JavaScript.

## A guided first look

```ez
# main.ez — a small demo touching several core features at once

task guessTheNumber(target, guess) {
    when guess == target {
        out "You guessed it! The number was " + str(target)
    } other when guess > target {
        out "Too high!"
    } other {
        out "Too low!"
    }
}

secretNumber = 42
myGuess = 50

out "Starting the game..."
guessTheNumber(secretNumber, myGuess)
```

Run it with:

```bash
ez main.ez
```

Behind that one command, EZ:

1. Reads `main.ez` and tokenizes it (`src/lexer/Lexer.cpp`).
2. Parses the tokens into an AST (`src/parser/Parser.cpp`).
3. Runs the optional type checker over the AST (a no-op here, since nothing
   is annotated).
4. Compiles the AST to bytecode (`src/compiler/BytecodeCompiler.cpp`).
5. Executes the bytecode on the stack VM (`src/vm/BytecodeVM_Execute.cpp`),
   dispatching `out`, `+`, `==`, and the function call through native
   opcodes.

If a runtime error occurs anywhere in this pipeline, EZ prints a stack trace
with file names and line numbers spanning every imported (`use`d) file, not
just the entry script.

## Where to go next

| I want to... | Read |
|---|---|
| See what EZ can do, end to end | [README](../README.md) |
| Install and run my first script | [getting-started.md](getting-started.md) |
| Learn the full grammar, keywords, and operators | [SYNTAX.md](../SYNTAX.md) |
| Look up a specific builtin function | [api.md](api.md) |
| Understand classes, interfaces, structs, enums | [object-oriented-programming.md](object-oriented-programming.md) |
| Understand `async`/`await`, threads, and channels | [08_Modules_and_Concurrency.md](08_Modules_and_Concurrency.md) |
| Call native libraries or build a GUI | [gui_readme.md](gui_readme.md) |
| Understand how the interpreter itself works internally | [ARCHITECTURE.md](../ARCHITECTURE.md) |
| See the `ezlib` package ecosystem | [09_Builtin_Libraries.md](09_Builtin_Libraries.md) |
| Get quick answers to common questions | [faq.md](faq.md) |
