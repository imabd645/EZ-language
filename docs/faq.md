# Frequently Asked Questions

## Syntax & design

### Why `when` instead of `if`, and `task` instead of `function`?
EZ prioritizes readable, English-like control flow: `when` reads as "when
this condition holds," and `other` naturally reads as the alternative.
`task` frames a unit of code as work to be done, which lines up with EZ's
native `async task`/`spawn` model for running work concurrently.

### Is EZ interpreted or compiled?
Both, in the sense most real language implementations are. Source is lexed,
parsed into an AST, optionally type-checked, then **compiled to a custom
bytecode**, which a stack-based virtual machine executes with computed-goto
dispatch. It is not a tree-walking interpreter that re-evaluates the AST
line by line — see [architecture.md](architecture.md) for the full
pipeline.

### Is EZ statically or dynamically typed?
Dynamically typed by default; every example that doesn't use `:` type
annotations runs exactly like a dynamic language. An **optional** static
type checker validates annotated code (`x: number`, `task f(a: number) ->
number`, `Array[T]`, `Dict[K, V]`) before compilation, and rejects type
errors with exit status 65 before any bytecode runs. See
[features.md](features.md#5-optional-static-type-checker).

### Does `${expr}` work in template strings?
No — EZ uses backtick templates with `{expr}`, e.g. `` `Hello {name}` ``,
not `` `Hello ${name}` ``.

### Why is `-2 ** 2` equal to `4`, not `-4`?
Unary minus binds *tighter* than `**` in EZ, unlike Python or standard
mathematical notation. Write `-(2 ** 2)` to negate the result of the power.
See [syntax.md](syntax.md#arithmetic).

## Platform

### Is EZ Windows-only?
No — the core language (lexer, parser, type checker, compiler, VM, GC,
event loop, threading, standard builtins, file I/O, HTTP client,
SQLite/FFI) is cross-platform and builds on Windows, Linux, and macOS.
`CMakeLists.txt` has a dedicated link path for non-Windows targets. **The
native GUI (`gui_*` builtins) is Windows-only**. All other parts of the
interpreter and standard library are fully cross-platform. See 
[architecture.md](architecture.md#platform-support) for the
full breakdown.

### Can I build a GUI app on Linux or macOS?
Not with the native `gui_*` builtins — those are Win32/GDI-specific and are
only compiled in on `WIN32` builds. Everything else (the language itself,
concurrency, networking, FFI to non-GUI libraries) works the same way it
does on Windows.

## Features

### Is EZ object-oriented?
Yes — class-based OOP via `model`, single inheritance (`extends`),
interfaces (`interface`/`implements`), encapsulation (`hidden`/`shown`),
static members, operator overloading, structs, and enums. See
[object-oriented-programming.md](object-oriented-programming.md).

### Can I build web apps in EZ?
The native runtime provides an HTTP **client** (`http_get`, `http_post`,
`fetch`) but no HTTP server and no `startServer`/`http_put`/`http_delete`
builtins. A web-server framework (`serve`) lives in the external `ezlib`
registry, built on top of the native concurrency and networking primitives.
See [standard-library.md](standard-library.md).

### Does EZ have a database driver built in?
No `db_*` builtins exist in the C++ runtime, even though `sqlite3` is
linked (it backs the `@persist` decorator). Full database/ORM support comes
from the `ezlib` `db`/`orm` package.

### How do I handle errors?
`try`/`catch`/`finally`, with typed catches (`catch (TypeName e)`) for
model-based exceptions, plus `throw` for any catchable value and `panic`
for uncatchable fatal errors. See [error-handling.md](error-handling.md).

```ez
try {
    fail_code()
} catch e {
    out "Error: " + str(e)
}
```

### How does concurrency work?
`spawn(fn, ...args)` for real OS threads, `async task`/`await` for
cooperative async I/O over a libuv event loop, plus `Mutex`, `Atomic`, and
`Channel` for coordinating shared state. See
[concurrency.md](concurrency.md).

### Can EZ call native/system libraries?
Yes, via the `os_*` FFI family — load a shared library, resolve a symbol,
and call it with up to 12 arguments (or use `os_call_sig` for
floating-point parameters), including callback support so native code can
call back into EZ. See [ffi-and-gui.md](ffi-and-gui.md).

### Can I ship a single executable?
Yes, `ez bundle entry.ez out.exe [--gui] [--icon app.ico]` crawls the `use`
dependency graph and packs every referenced file into the executable's own
binary as an embedded virtual file system. See
[getting-started.md](getting-started.md#bundling-a-standalone-executable).

## Performance & internals

### What garbage collector does EZ use?
A Bacon & Rajan (2001) reference-count cycle collector, layered on top of
ordinary `shared_ptr` refcounting — it exists specifically to catch
reference cycles that plain refcounting would leak, not to replace
refcounting for everything. See
[architecture.md](architecture.md#memory-management).

### How deep can recursion go?
Call depth is capped at 4096 frames by default, raising a catchable
"maximum call depth exceeded" error rather than crashing. Tail calls
(`give f(...)` in tail position) reuse the current frame instead of
growing the stack, supporting 20,000+ levels of tail-recursive calls. See
[features.md](features.md#3-functions).

### Where do I find the exact signature of a specific builtin?
[api.md](api.md) is the categorized overview; `BUILTINS.md` in the
repository root has a granular, source-cited entry (signature, return type,
example, and source file) for essentially every native function.
