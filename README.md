<div align="center">

<br>

```
███████╗███████╗    ██╗      █████╗ ███╗   ██╗ ██████╗
██╔════╝╚══███╔╝    ██║     ██╔══██╗████╗  ██║██╔════╝
█████╗    ███╔╝     ██║     ███████║██╔██╗ ██║██║  ███╗
██╔══╝   ███╔╝      ██║     ██╔══██║██║╚██╗██║██║   ██║
███████╗███████╗    ███████╗██║  ██║██║ ╚████║╚██████╔╝
╚══════╝╚══════╝    ╚══════╝╚═╝  ╚═╝╚═╝  ╚═══╝ ╚═════╝
```

**A dynamically-typed, bytecode-compiled programming language with natural English syntax**
**and an optional static type checker.**

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg?style=flat-square)](https://github.com/imabd645/EZ-language)
[![Language](https://img.shields.io/badge/written%20in-C%2B%2B17-orange.svg?style=flat-square)](https://github.com/imabd645/EZ-language)

[Quick Start](#-quick-start) · [Syntax Guide](#-language-syntax) · [Type Checker](#-optional-static-type-checker) · [Built-ins](#-built-in-functions-c-runtime) · [OOP](#-object-oriented-programming) · [Async](#-async--concurrency) · [Native FFI](#-native-ffi) · [GUI](#-gui-framework) · [Bundling](#-bundling-standalone-executables)

</div>

---



---

## What is EZ?

EZ is a scripting language built from scratch — bytecode compiler, stack VM, garbage collector, optional static type checker, and a Win32 GUI/FFI layer — all written in C++17. It replaces conventional keywords with plain English: `when` instead of `if`, `task` instead of `function`, `give` instead of `return`, `other` instead of `else`.

```ez
# A taste of EZ
task greet(name) {
    give "Hello, " + name + "!"
}

out greet("World")        # Hello, World!

people = ["Alice", "Bob", "Carol"]
get person in people {
    out greet(person)
}
```

---

## Table of Contents

- [Architecture](#-architecture)
- [Installation & Building](#-installation--building)
- [Quick Start](#-quick-start)
- [Language Syntax](#-language-syntax)
  - [Comments](#comments)
  - [Variables & Types](#variables--types)
  - [Strings & Interpolation](#strings--interpolation)
  - [Operators](#operators)
  - [Control Flow](#control-flow)
  - [Functions & Lambdas](#functions--lambdas)
  - [Arrays & Dictionaries](#arrays--dictionaries)
- [Optional Static Type Checker](#-optional-static-type-checker)
- [Object-Oriented Programming](#-object-oriented-programming)
  - [Enums](#enums)
  - [Operator Overloading](#operator-overloading)
- [Error Handling](#error-handling)
- [Modules (`use`)](#modules-use)
- [Built-in Functions (C++ Runtime)](#-built-in-functions-c-runtime)
- [Async & Concurrency](#-async--concurrency)
- [Native FFI](#-native-ffi)
- [GUI Framework (raw builtins)](#-gui-framework)
- [Bundling Standalone Executables](#-bundling-standalone-executables)
- [REPL](#-repl)
- [The `ezlib` Standard Library (external)](#-the-ezlib-standard-library-external)
- [Known Gaps & Caveats](#-known-gaps--caveats)
- [Contributing](#-contributing)

---

## 🏗 Architecture

EZ is designed to be extremely lightweight and blazingly fast.
- **Zero External Dependencies (Almost):** Everything from the Bytecode Virtual Machine, Lexer, Parser, and Memory Manager is custom-built from the ground up in modern C++17.
- **Generational Memory Management:** It automatically cleans up memory without freezing your application.
- **Built-in Async:** Networking and I/O tasks run on background threads automatically, keeping your programs highly responsive.
- **Fully Modular:** The codebase is designed for easy extensibility so you can drop in new features effortlessly.

## 🚀 Installation & Building

### Prerequisites

| Dependency | Purpose |
|---|---|
| C++17 compiler (MinGW-w64 / MSVC) | Building the interpreter |
| CMake 3.10+ | Build system |
| libcurl | HTTP client (`http_get`, `http_post`, `fetch`) |
| libsqlite3 | Linked by CMake. There are no `db_*` builtins — it backs `@persist` and is available to `ezlib` packages via FFI |
| Win32 SDK (`dwmapi`, `uxtheme`) | GUI dark-mode/theme APIs |

### Build with CMake

```bash
git clone https://github.com/imabd645/EZ-language.git
cd EZ-language

mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .
```

This produces `ez.exe` and links against `sqlite3 curl ws2_32 pthread` (plus `dwmapi uxtheme` on Windows), per `CMakeLists.txt`.

### Build directly with batch script (Windows)

We provide an automated build script out of the box.

```bash
build.bat
```
This will automatically invoke the compiler, statically link all dependencies (like cURL and SQLite), and output a single `ez.exe` executable!

### Add to PATH

1. Move `ez.exe` to a permanent folder (e.g. `C:\ez\`)
2. Open **System Properties → Environment Variables → Path → Edit → New**
3. Add `C:\ez`
4. Restart your terminal

```bash
ez --help        # show usage
ez hello.ez      # run a script
ez --trace hello.ez   # run with bytecode execution tracing
ez               # start the REPL
```

---

## 🎓 Quick Start

### Hello World

```ez
out "Hello, World!"
```

```bash
ez hello.ez
# Hello, World!
```

### Five Minutes of EZ

```ez
# Variables — no declaration keyword needed
name = "Abdullah"
age  = 19
pi   = 3.14159
active = true

out "Name: " + name
out "Age:  " + str(age)

# Conditional
when age >= 18 {
    out name + " is an adult"
} other {
    out name + " is a minor"
}

# repeat i = N to M — inclusive on both ends, auto-detects reverse direction
repeat i = 1 to 5 {
    out "Count: " + str(i)
}

# For-each loop
fruits = ["apple", "banana", "cherry"]
get fruit in fruits {
    out fruit
}

# Function
task square(n) {
    give n * n
}

out str(square(7))    # 49

# Lambda
double = |x| x * 2
out str(double(5))    # 10
```

---

## 📚 Language Syntax

### Comments

EZ supports three comment styles:

```ez
# Hash comment (line)
// Double-slash comment (line)
/* Block comment, supports /* nesting */ */
```

### Variables & Types

Variables are dynamically typed by default (optional static types are covered in the [Type Checker](#-optional-static-type-checker) section below). No declaration keyword is needed — just assign.

```ez
# Numbers — two internal runtime types: INTEGER (long long) and NUMBER (double)
x     = 42          # INTEGER
pi    = 3.14159     # NUMBER (double)
big   = 0xFF        # hex literal -> INTEGER (255)

# Strings (single or double quotes — both behave the same)
msg   = "Hello, EZ!"
msg2  = 'also fine'

# Raw strings — prefix r before a quote disables escape processing
raw   = r"C:\Users\file.txt"

# Booleans — yes/no are aliases for true/false
flag1 = true
flag2 = yes
flag3 = false
flag4 = no

# Nil
nothing = nil

# Compound assignment
x += 5      # x = x + 5
x -= 2
x *= 3
x /= 4
```

**Runtime type names** (returned by `typeOf()`): `"nil"`, `"bool"`, `"integer"`, `"float"`, `"string"`, `"array"`, `"dictionary"`, `"function"`, `"model"`, `"instance"`, `"future"`, `"buffer"`, `"mutex"`, `"interface"`, `"super"`.

> Note: `typeOf` distinguishes `"integer"` (whole numbers, stored as `long long` for fast arithmetic) from `"float"` (doubles). Both satisfy `isNumber()` checks in the runtime.

### Strings & Interpolation

EZ has **three** string forms:

```ez
"double quoted"     # supports escapes: \n \t \r \0 \\ \" \' and \xNN (hex byte)
'single quoted'     # identical semantics to double quotes
r"raw string"       # backslashes are literal — no escape processing
```

**Template strings use backticks with `{expr}` interpolation** (not `${...}`):

```ez
name = "Abdullah"
age  = 19
out `Hello {name}, you are {age + 1} next year`
# -> Hello Abdullah, you are 20 next year
```

Internally the lexer desugars a backtick template into string concatenation with `str(...)` calls around each `{...}` expression — so `` `A{1+1}B` `` becomes `("A" + str(1+1) + "B")`. A backtick string with no `{}` interpolation is just a plain string literal.

### Operators

```ez
# Arithmetic
out 10 + 3      # 13
out 10 - 3      # 7
out 10 * 3      # 30
out 10 / 3      # 3.333...
out 10 % 3      # 1  (modulo)

# Comparison
out 5 == 5      # true
out 5 != 4      # true
out 3 <  4      # true
out 4 <= 4      # true
out 5 >  3      # true
out 5 >= 5      # true

# Logical
out true and false    # false
out true or  false    # true
out not true          # false

# Bitwise
out 10 & 12           # 8   (AND)
out 10 | 5            # 15  (OR)
out 10 ^ 12           # 6   (XOR)
out ~5                 # bitwise NOT
out 1 << 3             # 8   (left shift)
out 16 >> 2            # 4   (right shift)

# Ternary
label = age >= 18 ? "adult" : "minor"

# Spread (in array literals and function calls)
a = [1, 2, 3]
b = [...a, 4, 5]      # [1, 2, 3, 4, 5]
```

> **`**` is not a real operator.** Although a `POW` opcode exists in the bytecode ISA, the lexer never produces a `**`/power token — there is only a single `*` (multiply) operator. For exponentiation, use the `pow(base, exp)` builtin. Writing `2 ** 8` will lex as two consecutive `*` tokens and will not parse as exponentiation.

> **`0b...` binary literals are not supported** by the lexer — only decimal numbers and `0x...` hexadecimal literals are recognized.

### Control Flow

#### `when` / `other when` / `other` (if / else if / else)

```ez
score = 85

when score >= 90 {
    out "A"
} other when score >= 80 {
    out "B"
} other when score >= 70 {
    out "C"
} other {
    out "F"
}
```

`other when ...` is parsed as a single nested `when` statement attached as the `else` branch — this is the **only** way to chain conditions. A bare `when` block immediately followed by another `when` block (without `other`) is parsed as **two independent statements**, not an if/else-if chain.

EZ also supports a brace-less single-statement form for short conditionals:

```ez
when x == 5 { escape }
when x % 2 == 0 { skip }
```

#### `while` loop

```ez
n = 1
while n <= 10 {
    out str(n)
    n += 1
}
```

#### `repeat i = N to M` (inclusive range loop)

The compiler emits `loopVar <= endVar` as the loop condition — **both bounds are inclusive**. If `N > M` (and both are literal constants), the compiler automatically detects this and emits a decrementing loop (`loopVar >= endVar`) instead.

```ez
repeat i = 1 to 5 {
    out str(i)     # prints 1 2 3 4 5
}

repeat i = 5 to 1 {
    out str(i)     # prints 5 4 3 2 1 (auto reverse)
}
```

#### `get ... in ...` (for-each)

```ez
# Iterate an array
get item in ["alpha", "beta", "gamma"] {
    out item
}

# Iterate dictionary keys
config = {"host": "localhost", "port": 8080}
get key in config {
    out key + " = " + str(config[key])
}

# Destructure key AND value
get [k, v] in config {
    out k + " = " + str(v)
}
```

#### `match` (pattern matching)

```ez
state = "success"
match state {
    "loading" => out "Please wait..."
    "success" => {
        out "Operation completed!"
    }
    other => out "Unknown state"
}
```

Each arm is `<expression> => <statement-or-block>`; the special `other` arm (no expression) is the default/fallback case. There is no support for value ranges, multiple values per arm, or destructuring patterns in `match` — each arm pattern is evaluated and compared with `==` against the subject.

#### `escape` and `skip`

- `escape` — break out of the nearest enclosing loop (`while`, `repeat`, `get`).
- `skip` — continue to the next iteration.

```ez
repeat i = 1 to 10 {
    when i == 5 { escape }
    when i % 2 == 0 { skip }
    out str(i)
}
```

### Functions & Lambdas

```ez
# Basic function
task add(a, b) {
    give a + b
}
out str(add(3, 4))    # 7

# Default parameters
task greet(name, greeting) {
    when not greeting { greeting = "Hello" }
    give greeting + ", " + name + "!"
}

# Variadic functions (... prefix on the last parameter)
task sum(...nums) {
    total = 0
    get n in nums { total += n }
    give total
}
out str(sum(1, 2, 3, 4, 5))    # 15

# Recursive function
task factorial(n) {
    when n <= 1 { give 1 }
    give n * factorial(n - 1)
}

# Tail-call optimized recursion (give f(...) in tail position reuses
# the current stack frame — supports 20,000+ levels of recursion)
task count(n, acc) {
    when n == 0 { give acc }
    give count(n - 1, acc + 1)    # TCO
}

# Lambda — single expression
double = |x| x * 2

# Lambda — multi-statement body
clamp = |val, lo, hi| {
    when val < lo { give lo }
    when val > hi { give hi }
    give val
}

# Closures — Lua-style upvalues, migrate to the heap when their
# enclosing scope exits
task makeCounter() {
    count = 0
    give || {
        count += 1
        give count
    }
}
counter = makeCounter()
out str(counter())    # 1
out str(counter())    # 2
out str(counter())    # 3
```

#### `static` locals (persistent variables across calls)

A `static name = expr` statement inside a `task` body declares a variable whose initializer runs **only on the first call** — its value persists across subsequent calls to the same function, similar to a `static` local in C:

```ez
task tick() {
    static count = 0
    count += 1
    give count
}

out str(tick())   # 1
out str(tick())   # 2
out str(tick())   # 3
```

Internally each `static` variable is compiled to a uniquely-mangled global (`__static_<compilerId>_<funcName>_<varName>`) and is only initialized the first time the declaration executes (checked via a `HAS_GLOBAL` test).

### Arrays & Dictionaries

```ez
# Array literals
primes = [2, 3, 5, 7, 11]

# Indexing (0-based, supports negative indices from the end)
out str(primes[0])      # 2
out str(primes[-1])     # 11

# Array operations
push(primes, 13)
pop(primes)
out str(len(primes))    # 5

# Slice
out str(primes[1:3])    # [3, 5]

# Spread into function call
args = [3, 4]
out str(add(...args))

# Dictionary (hash map) — keys are strings
user = {
    "name": "Alice",
    "age":  30,
    "tags": ["admin", "user"]
}

out user["name"]                    # Alice
out str(user["age"])                # 30
out str(has_key(user, "email"))     # false

user["email"] = "alice@example.com"

get key in user {
    out key + ": " + str(user[key])
}

out str(keys(user))     # ["name", "age", "tags", "email"]
out str(len(user))      # 4
dictRemove(user, "tags")
```

---

## 🔍 Optional Static Type Checker

EZ ships with an opt-in static type checker (`TypeChecker.cpp`, ~673 lines) that runs **after parsing and before bytecode compilation**. If it reports an error, the script exits with status 65 before any code runs. Type annotations are entirely optional — untyped code (everything shown above) checks fine because unannotated variables default to type `Any`, which is compatible with everything.

### Type annotation syntax

```ez
# Variable declarations with type annotations
x: bool = true
i: number = 0
arr: Array[number] = [1, 2, 3]
dict: Dict[string, number] = {"a": 1, "b": 2}

# Function/task signatures
task add(a: number, b: number) -> number {
    give a + b
}

# Struct fields with types and defaults
struct User {
    name: string
    age: number = 25
}

# Interface method signatures
interface Logger {
    task log(message: string, level: number) -> bool
}
```

### What the type checker validates

- **Type mismatches** in variable declarations (`arr[1] = "string"` where `arr: Array[number]` → error).
- **Dictionary key/value types**: `dict["key"]` must match the declared `Dict[K, V]` key type, e.g. `dict[1] = 2` on a `Dict[string, number]` is an error.
- **Logical operator operands**: `and`/`or` operands must be `bool` (`x and "string"` → error).
- **`self` usage**: `self` can only be referenced inside a `model`'s `init` or methods — using `self` at top-level is an error.
- **Loop control statements**: `escape`/`skip` (break/continue) outside of any loop is an error.
- **Generic container types**: `Array[T]` and `Dict[K, V]` with nested type arguments are supported in the type grammar (`TypeInfo` / `TypeAST`).
- Any type written as `Any` (or omitted) is treated as compatible with everything — it's the universal escape hatch for dynamic code.

If you never write `:` type annotations, this entire pass is effectively a no-op pass-through, and EZ behaves as a purely dynamically-typed language.

---

## 🧱 Object-Oriented Programming

### Models (Classes)

```ez
model Animal {
    init(name, sound) {
        self.name  = name
        self.sound = sound
    }

    # Public method
    shown speak() {
        out self.name + " says " + self.sound
    }

    # Private method
    hidden _describe() {
        give "I am " + self.name
    }

    # toString override — called by out and str()
    task toString() {
        give "Animal(" + self.name + ")"
    }
}

cat = Animal("Cat", "meow")
cat.speak()          # Cat says meow
out str(cat)         # Animal(Cat)
```

### Inheritance

```ez
model Dog extends Animal {
    init(name) {
        super.init(name, "woof")
        self.tricks = []
    }

    task learnTrick(trick) {
        push(self.tricks, trick)
        give self    # enables method chaining
    }

    shown perform() {
        get trick in self.tricks {
            out self.name + " performs: " + trick
        }
    }
}

rex = Dog("Rex")
rex.learnTrick("sit").learnTrick("roll over")
rex.speak()      # Rex says woof
rex.perform()    # Rex performs: sit
                 # Rex performs: roll over
```

When a class `extends` a parent, all of the parent's methods are copied into the child's method table at class-creation time (unless overridden) — inheritance is implemented as a one-time method-table merge, not a runtime lookup chain.

### Interfaces

```ez
interface Serializable {
    task toJson()
    task fromJson(json)
}

model Config implements Serializable {
    init(data) {
        self.data = data
    }

    task toJson() {
        give to_json(self.data)
    }

    task fromJson(json) {
        self.data = parse_json(json)
        give self
    }
}

cfg = Config({"debug": true, "port": 3000})
out cfg.toJson()
```

When a model with `implements SomeInterface` is created, the VM checks (at class-creation time) that every method named in the interface exists in the class's method table. If a required method is missing, EZ raises a runtime error: `Model 'X' fails to implement interface 'Serializable': missing task 'fromJson'`. This is presence-only validation — argument and return types of interface methods are not checked at runtime.

### Static Members

```ez
model Counter {
    static count = 0

    init() {
        Counter.count += 1
        self.id = Counter.count
    }

    static task reset() {
        Counter.count = 0
    }

    static task total() {
        give Counter.count
    }
}

a = Counter()
b = Counter()
c = Counter()
out str(Counter.total())    # 3
Counter.reset()
out str(Counter.total())    # 0
```

### Structs

```ez
struct Point {
    x, y
}

p = new Point()
p.x = 10
p.y = 20
out str(p.x) + ", " + str(p.y)
```

Structs may also carry type annotations and default values when used with the [type checker](#-optional-static-type-checker):

```ez
struct User {
    name: string
    age: number = 25
}
```

### Enums

`enum` declares a set of named integer constants. Members are read as
`Name.MEMBER`.

```ez
enum Color { RED, GREEN, BLUE }          # 0, 1, 2

out str(Color.GREEN)                     # 1
```

Numbering starts at 0 and increments. An explicit value re-seeds the counter
rather than restarting it, so members after one keep counting from there:

```ez
enum Status {
    OK = 200,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
}

enum Seeded { A = 5, B, C }              # 5, 6, 7
```

Commas are optional — members may be separated by commas, newlines, or both, and
a trailing comma is fine:

```ez
enum Direction {
    NORTH,
    EAST,
    SOUTH,
    WEST,
}
```

Members are ordinary numbers, so they compare, do arithmetic, and live in arrays
and dictionaries like any other value:

```ez
when code == Status.NOT_FOUND { out "missing" }
palette = [Color.RED, Color.GREEN]
```

An enum desugars to a model whose members are all static, which is why the access
syntax matches [static members](#static-members). Two mistakes are rejected at
parse time: a **duplicate member name** (it would silently shadow the earlier one,
leaving no way to tell which value you were getting) and an **empty enum body**
(almost always an unfinished edit).

### Operator Overloading

The parser allows **any token** to be used as a function name inside a `model` body — this is what enables operator overloading. The bytecode VM checks for a matching method on a model instance before falling back to the built-in numeric/structural behavior.

**Overloadable** (looked up by name on the left-hand instance):

| Operator | Method name | Notes |
|---|---|---|
| `+` | `task +(other)` | |
| `-` (binary) | `task -(other)` | |
| `*` | `task *(other)` | |
| `/` | `task /(other)` | |
| `==` | `task ==(other)` | |
| `<` | `task <(other)` | |
| `>` | `task >(other)` | |
| `>=` | `task >=(other)` | |
| `<=` | `task <=(other)` | |
| `!=` | `task !=(other)` | |
| `-` (unary negation) | `task neg()` | invoked for `-v` |

```ez
model Vector {
    init(x, y) {
        self.x = x
        self.y = y
    }

    task +(other) {
        give Vector(self.x + other.x, self.y + other.y)
    }

    task ==(other) {
        give self.x == other.x and self.y == other.y
    }

    task neg() {
        give Vector(-self.x, -self.y)
    }

    task toString() {
        give "Vector(" + str(self.x) + ", " + str(self.y) + ")"
    }
}

v1 = Vector(10, 20)
v2 = Vector(5, 5)
v3 = v1 + v2          # Vector(15, 25)
v1 += v2              # desugars to v1 = v1 + v2
out str(v1 == v3)     # true
v4 = -v2              # Vector(-5, -5)
```

---

## Error Handling

### `try` / `catch` / `throw`

```ez
task divide(a, b) {
    when b == 0 {
        throw "Division by zero"
    }
    give a / b
}

try {
    result = divide(10, 0)
} catch e {
    out "Caught: " + str(e)    # Caught: Division by zero
}
```

### Typed catches and model-based exceptions

`catch (TypeName e)` matches only if the thrown value is an instance of `TypeName` (or one of its subclasses, via `extends`). Multiple typed `catch` clauses can be chained, with an untyped `catch e` as a final catch-all:

```ez
model MathError {
    init(msg) { self.msg = msg }
}

model NetworkError {
    init(msg) { self.msg = msg }
}

try {
    throw MathError("division by zero")
} catch (MathError e) {
    out "Math error: " + e.msg
} catch (NetworkError e) {
    out "Network error: " + e.msg
} catch e {
    out "Unknown: " + str(e)
}
```

Subclasses (`model DerivedError extends MathError`) are caught by a `catch (MathError e)` clause as well — the VM walks the instance's class chain when matching catch types.

### Built-in `Exception` helper

A native `Exception(message, code)` constructor returns a plain dictionary with `message`, `code`, and `stackTrace` keys — useful as a lightweight, allocation-free alternative to defining your own error models:

```ez
err = Exception("Not found", 404)
out err["message"]   # Not found
out str(err["code"]) # 404
```

### Nested try/catch and re-throw

```ez
try {
    try {
        throw "inner"
    } catch e {
        out "Inner handler: " + str(e)
        throw "re-thrown"
    }
} catch e {
    out "Outer handler: " + str(e)
}
```

### `panic`

`panic(message)` immediately raises a fatal runtime error (the same mechanism used for internal VM errors) — it is not catchable by `try`/`catch` and terminates the script with exit code 70.

---

## Modules (`use`)

```ez
# Import entire file — all its globals become available
use "lib/utils.ez"

# Namespaced import — access via alias.name
use "lib/math.ez" as math
out str(math.sqrt(25))

# Import everything from a module into the current scope
use "lib/helpers.ez" as *

# Import an installed package (after `ez install <name>`)
use "collections"
```

`use` resolves paths relative to `lib/<name>.ez`, `lib/<name>/main.ez`, `C:/ezlib/<name>.ez`, or `C:/ezlib/<name>/main.ez`, in addition to the literal path given. This same resolution logic is also how `ez bundle` discovers transitive dependencies to pack into a standalone executable.

---

## 🔧 Built-in Functions (C++ Runtime)

Everything in this section is registered directly as a native function in the C++ source (`src/Builtins.cpp` and `src/runtime/Builtins_*.cpp`) and is always available, with no `use` statement required.

### Output, Input & Console

| Function | Description |
|---|---|
| `out expr` | Print value with newline (language statement, not a function call) |
| `print(val)` | Print without a trailing newline |
| `input(prompt)` / `__input__` | Read a line from stdin |
| `color(code)` | Set Windows console text color |
| `reset()` | Reset console color |
| `clear()` | Clear the console screen |
| `gotoxy(x, y)` | Move console cursor |
| `getch()` | Read a single character without echo |

### Type Conversion & Inspection

| Function | Description |
|---|---|
| `str(val)` | Convert any value to string |
| `num(val)` | Parse string/value to number |
| `typeOf(val)` / `type(val)` | Return the runtime type name as a string |
| `len(val)` | Length of string, array, or dictionary |

### String Functions

| Function | Description |
|---|---|
| `substr(s, start, len)` / `substring(s, start, end)` | Extract substring |
| `split(s, delim)` | Split string to array |
| `join(arr, delim)` | Join array to string |
| `upper(s)` / `toUpper(s)` | Uppercase |
| `lower(s)` / `toLower(s)` | Lowercase |
| `trim(s)` | Strip leading/trailing whitespace |
| `replace(s, old, new)` | Replace substring |
| `contains(s, sub)` | Substring check (also works on arrays for membership) |
| `startsWith(s, prefix)` | Prefix check |
| `endsWith(s, suffix)` | Suffix check |
| `indexOf(s_or_arr, val)` | First index, or -1 |
| `ord(char)` | Character → ASCII code |
| `chr(code)` | ASCII code → character |

### Array & Dictionary Functions

| Function | Description |
|---|---|
| `push(arr, val)` | Append to array |
| `pop(arr)` | Remove and return last element |
| `insert(arr, idx, val)` | Insert at index |
| `remove(arr, val_or_idx)` | Remove an element |
| `reverse(arr)` | Reverse in place |
| `sort(arr)` | Sort in place |
| `slice(arr, start, end)` | Return sub-array |
| `range(start, end[, step])` | Generate an array of numbers |
| `filter(arr, fn)` | Return elements where `fn` returns true |
| `map(arr, fn)` | Transform each element |
| `reduce(arr, fn, init)` | Fold array to a single value |
| `forEach(arr, fn)` | Call `fn` for each element (no return value) |
| `every(arr, fn)` | True if `fn` is true for all elements |
| `some(arr, fn)` | True if `fn` is true for any element |
| `find(arr, fn)` | First element matching `fn`, or `nil` |
| `keys(dict)` | Array of dictionary keys |
| `values(dict)` | Array of dictionary values |
| `has_key(dict, key)` | Boolean key check |
| `dictRemove(dict, key)` | Delete a key |

### JSON

| Function | Description |
|---|---|
| `parse_json(str)` | Parse a JSON string to an EZ value (via `MiniJson.h`) |
| `to_json(val)` | Serialize an EZ value to a JSON string |

### File I/O

| Function | Description |
|---|---|
| `readFile(path)` | Read entire file as a string |
| `writeFile(path, content)` | Write/overwrite a file |
| `appendFile(path, content)` | Append to a file |
| `readLines(path)` | Read file as an array of lines |
| `writeLine(path, line)` | Write a single line |
| `appendLine(path, line)` | Append a single line |

#### The `File` class

For anything beyond read-it-all / write-it-all — streaming, appending over time,
seeking, or managing files on disk — use the `File` class.

```ez
f = File("app.log", "a")     # "r" "w" "a", or "rb" "wb" "ab", or "rw"
f.write("a line\n")
f.flush()                    # push to disk without closing
f.close()
```

`File()` **throws** `FileNotFoundError` if the path cannot be opened — it never
returns a closed handle, so wrap it in `try` rather than testing the result:

```ez
try {
    f = File("missing/dir/x.txt", "a")
} catch (e) {
    out "could not open: " + str(e)
}
```

| Method | Description |
|---|---|
| `readLine()` | One line without its newline; `nil` at EOF |
| `read(n)` | Up to `n` bytes; `""` at EOF |
| `readAll()` | Everything remaining |
| `write(s)` / `writeLine(s)` | Write, optionally with a trailing newline |
| `flush()` | Push buffered writes to the OS — without it, recent writes sit in the stream buffer and are lost if the process dies |
| `seek(offset)` / `tell()` | Move / report the file position |
| `size()` | Size in bytes; restores the position, so it is safe mid-write |
| `isOpen()` / `eof()` | State checks |
| `close()` | Close the handle |

Path-level operations are **static** — they act on a path, not an open handle:

| Static | Description |
|---|---|
| `File.exists(path)` | Does it exist? |
| `File.size(path)` | Size in bytes |
| `File.rename(from, to)` | Rename, replacing the destination if present |
| `File.remove(path)` | Delete. Returns `false` if it was already absent, so cleanup needs no guard; throws only on a real failure (e.g. permissions) |
| `File.delete(path)` | Alias for `File.remove` |

### HTTP Client (via libcurl)

| Function | Description |
|---|---|
| `http_get(url[, headers])` | HTTP GET |
| `http_post(url, body[, headers])` | HTTP POST |
| `fetch(url[, options])` | Generic HTTP request helper |
| `url_encode(str)` / `url_decode(str)` | URL component encoding |

> `http_put`, `http_delete`, and `startServer(port, handler)` (web server) are **not** present as C++ builtins in this repository.

### Regular Expressions

The engine is `std::regex` in ECMAScript mode. It has **no dotall and no named
groups**, so neither is offered.

The `re_*` family reports match **positions**, which the older `re*` functions do
not. That matters: without offsets there is no correct way to walk a string —
locating a match by searching for its own text finds the first *literal*
occurrence instead (`\bcat\b` against `"concat cat"` lands on index 3, not 7),
and a zero-width match never advances.

| Function | Description |
|---|---|
| `re_test(text, pattern[, flags])` | Does the pattern occur anywhere? |
| `re_full_match(text, pattern[, flags])` | Does it match the whole string? |
| `re_find(text, pattern[, flags, start])` | First match at or after `start`, else `nil` |
| `re_find_all(text, pattern[, flags, limit])` | Every non-overlapping match |
| `re_replace(text, pattern, repl[, flags, limit])` | Replace; `limit` 0 = all. `$1`–`$9` and `$&` work in `repl` |
| `re_split(text, pattern[, flags, limit])` | Split on the pattern |
| `re_escape(text)` | Escape metacharacters to match text literally |
| `re_valid(pattern)` | Does the pattern compile? |

`re_find` / `re_find_all` return dictionaries shaped
`{"text", "start", "end", "groups"}`. A capture group that did not participate is
`nil`, not `""` — the only way to tell `(a)?` that failed to match from one that
matched empty.

An invalid pattern throws `RegexError` rather than reporting "no match", which is
the worst failure mode a validator can have. Zero-width matches advance by one
character, so `a*` terminates.

Flags are a string and combine: `"i"` case-insensitive, `"m"` multiline
(`^`/`$` also match at line breaks). An unknown flag is an error.

```ez
m = re_find("2026-08-07", "([0-9]{4})-([0-9]{2})")
m["text"]        # "2026-08"
m["start"]       # 0
m["groups"][0]   # "2026"

re_replace("John Smith", "(\\w+) (\\w+)", "$2, $1")   # "Smith, John"
```

The original three remain for compatibility; they return matched strings only,
with no positions and no flags:

| Function | Description |
|---|---|
| `reMatch(text, pattern)` | Test if `text` matches `pattern` entirely |
| `reSearch(text, pattern)` | First match, as an array of `[whole, group1, …]` |
| `reReplace(text, pattern, repl)` | Replace all matches |

### Math Functions

| Function | Description |
|---|---|
| `floor(x)` | Round down |
| `ceil(x)` | Round up |
| `round(x)` | Round to nearest integer |
| `abs(x)` | Absolute value |
| `sqrt(x)` | Square root |
| `pow(base, exp)` | Exponentiation (use this instead of `**`) |
| `min(a, b)` | Minimum of two values |
| `max(a, b)` | Maximum of two values |
| `rand()` | Random float in `[0, 1)` |
| `randint(lo, hi)` | Random integer in range |

### Buffers (raw byte storage)

| Function | Description |
|---|---|
| `buffer(size_or_string)` | Allocate a raw byte buffer, or build one from a string |
| `buf_size(buf)` | Buffer length in bytes |
| `buf_fill(buf, byteVal)` | Fill the buffer with a byte value |
| `buf_copy(src, dst, ...)` | Bulk-copy bytes between buffers |
| `buf_to_str(buf)` | Decode buffer contents as a UTF-8 string |
| `buf[i]` / `buf[i] = v` | Index a buffer directly to read/write a byte |

### Concurrency Primitives

| Function | Description |
|---|---|
| `spawn(fn, ...args)` | Start a detached OS thread running `fn(...args)` → `Future` |
| `await expr` / `sync expr` | Block until a `Future` resolves (`sync` is an alias for `await` as a function call) |
| `awaitAll(futures)` | Block until every future resolves; results in input order |
| `awaitAny(futures)` | Block until the **first** future settles, and return it |
| `isDone(future)` | Has it finished? Non-blocking — for progress reporting |
| `cancel(future)` | Cancel a future; awaiting it then throws |
| `waitAsync(ms)` | Non-blocking delay (yields to the event loop) |
| `wait(ms)` / `stop(ms)` | Blocking sleep for `ms` milliseconds |
| `mutex()` | Create a `Mutex` value |
| `lock(mu, fn)` | Acquire mutex, run `fn`, release (RAII — releases even if `fn` throws) |
| `Atomic(initial)` | Atomic integer: `get()`, `set(v)`, `add(n)`, `sub(n)` |
| `Channel()` | Blocking queue — see below |

If a spawned task throws, the failure is recorded **on its future**: awaiting it
re-raises the error, so it can be caught at the call site.

```ez
try {
    await(spawn(mightFail))
} catch (e) {
    out "task failed: " + str(e.message)
}
```

An error that crosses a future boundary arrives as an exception **instance** — read
`e.message` for the text, since `str()` on an instance gives `<instance>`. A local
`throw "text"` is caught as a plain string.

#### `Channel` — blocking queue

A real mutex plus condition variable, for handing values between threads without
polling.

| Method | Description |
|---|---|
| `send(value)` | Append a value and wake one receiver |
| `receive()` | Block until a value is available; `nil` if the channel is closed and drained |
| `receiveTimeout(ms)` | Block up to `ms`; `nil` on timeout |
| `tryReceive()` | Take a value only if one is queued; never blocks |
| `size()` | Values currently queued |
| `close()` / `isClosed()` | Close, waking every receiver |

Since a queued `nil` is indistinguishable from "empty", send a token value rather
than `nil` when using a channel to signal.

### Date & Time

`clock()` gives raw epoch milliseconds. For calendar work use `DateTime`.

```ez
d = DateTime()                              # now
d.format("%Y-%m-%d %H:%M:%S")               # "2026-08-07 01:32:51"
d.timestamp()                               # epoch milliseconds

birthday = DateTime(1998, 6, 12)            # y, m, d
launch   = DateTime(2026, 8, 7, 9, 30, 0)   # y, m, d, h, min, s
```

| Method | Description |
|---|---|
| `year()` `month()` `day()` | Date parts |
| `hour()` `minute()` `second()` | Time parts |
| `weekday()` | Day of week |
| `timestamp()` | Epoch milliseconds |
| `format(fmt)` | `strftime` pattern, rendered in local time |
| `diff(other)` | Milliseconds between two `DateTime`s |
| `addMs(n)` / `addSeconds(n)` / `addDays(n)` | Shifted copy |
| `toString()` | Readable form |

`DateTime()` accepts 0, 3, or 6 arguments; anything else is a `TypeError`, and an
impossible date is a `ValueError`.

`Timer` runs a callback on a background thread:

```ez
t = Timer(1000, true)          # every second, repeating
t.onTick(| | { out "tick" })
t.start()
# ...
t.stop()
```

| Method | Description |
|---|---|
| `onTick(fn)` | Set the callback (chainable) |
| `start()` / `stop()` | Start / stop the background thread |
| `isRunning()` | State check |

### Metaprogramming

| Function | Description |
|---|---|
| `getattr(obj, name)` | Get property by name string |
| `setattr(obj, name, val)` | Set property by name string |
| `hasattr(obj, name)` | Check if property exists |

### Errors & Process Control

| Function | Description |
|---|---|
| `Exception(message, code)` | Construct a `{message, code, stackTrace}` dictionary |
| `panic(message)` | Raise a fatal, uncatchable runtime error |
| `exit(code)` | Terminate the process immediately |
| `clock()` | Milliseconds since the Unix epoch (wall-clock, **not** process start) |

---

## ⚡ Async & Concurrency

EZ's concurrency model combines a custom native **event loop** for cooperative async/await with **real OS threads** (`spawn`) for CPU-bound parallel work, plus thread-safe primitives (`Mutex`, `Atomic`).

### `async task` + `await`

```ez
async task fetchUser(id) {
    data = http_get("https://api.example.com/users/" + str(id))
    give data
}

f1 = fetchUser(1)
f2 = fetchUser(2)

out "Fetching in parallel..."

r1 = await f1
r2 = await f2
```

### `async { ... }` blocks

```ez
result = await async {
    wait(500)
    give "done"
}

# Inline await
out await fetchUser(3)
```

### Thread-safe shared state with `Mutex`

```ez
model SafeCounter {
    init() {
        self.mu    = mutex()
        self.value = 0
    }

    task increment() {
        lock(self.mu, || {
            self.value += 1
        })
    }

    task get() {
        give self.value
    }
}

counter = SafeCounter()

futures = []
repeat i = 1 to 10 {
    f = spawn(|| { counter.increment() })
    push(futures, f)
}

get f in futures { await f }
out str(counter.get())    # 10
```

### Notes on `spawn`

When `spawn(fn, ...args)` launches a new OS thread, the VM exports a snapshot of thread state (`exportThreadState`) and closes any upvalues captured by `fn` so the worker thread doesn't hold dangling pointers into the parent VM's stack. Shared mutable state (arrays, dictionaries, model instances) is `shared_ptr`-based and remains shared across threads — this is why `Mutex` and `Atomic` exist, to coordinate access to that shared state safely. The garbage collector coordinates a stop-the-world pause across all active VM threads (`active_vm_threads`) before each collection.

---

## 🔗 Native FFI

EZ can call into arbitrary Windows DLL exports through a family of `os_*` builtins. This is a **low-level** interface — there is no automatic argument marshalling beyond numbers, strings, booleans, and buffers, and calls are wrapped in a SEH/vectored-exception guard so a bad call raises an EZ runtime error instead of crashing the interpreter.

### Loading a library and resolving a function pointer

```ez
kernel = os_load_lib("kernel32.dll")
sleepFn = os_get_func(kernel, "Sleep")
```

### Calling a function: `os_call(funcPtr, returnType, ...args)`

`returnType` is one of `"int"`, `"float"`, `"ptr"`, or `"string"`. Up to **12** arguments are supported; each argument is coerced based on its EZ type — numbers become `intptr_t`, strings become `const char*` (via `.c_str()`), buffers become pointers to their backing storage, and booleans become `0`/`1`.

```ez
kernel = os_load_lib("kernel32.dll")
sleepFn = os_get_func(kernel, "Sleep")
os_call(sleepFn, "int", 1000)   # Sleep(1000) — pause for 1 second

user32 = os_load_lib("user32.dll")
msgBox = os_get_func(user32, "MessageBoxA")
os_call(msgBox, "int", 0, "Hello from EZ!", "EZ FFI Demo", 0)
```

If `funcPtr` is null (library/function not found), or if the call itself crashes (access violation), `os_call` raises a catchable EZ runtime error rather than terminating the process.

### Raw memory access

For working with structs and pointers returned by Win32 APIs, EZ exposes direct memory read/write builtins:

| Function | Description |
|---|---|
| `os_alloc(size)` / `os_free(ptr)` | Allocate/free a raw memory block, returns its address as an integer |
| `os_read_byte(ptr, offset)` / `os_write_byte(ptr, offset, val)` | Single byte |
| `os_read_uint16/32/64(ptr, offset)` / `os_write_uint16/32/64(ptr, offset, val)` | Unsigned integers of various widths |
| `os_read_float32/64(ptr, offset)` / `os_write_float32/64(ptr, offset, val)` | Floats/doubles |
| `os_read_string_ptr(ptr)` | Read a null-terminated C string from a pointer |
| `os_write_string(ptr, offset, str)` | Write a string into memory |
| `os_buffer_from_ptr(ptr, size)` | Wrap a raw pointer as an EZ `buffer` |
| `os_buffer_addr(buf)` | Get the backing address of an EZ `buffer` |

### Struct layout helpers

| Function | Description |
|---|---|
| `os_struct_alloc(fieldTypeNames)` | Compute and allocate a buffer sized/aligned for a struct described by an array of type-name strings |
| `os_struct_pack(layout, values[, buffer])` | Pack EZ values into a buffer according to a layout description |
| `os_struct_unpack(layout, bufferOrPtr)` | Unpack a buffer/pointer into an array of EZ values according to a layout |

### Misc

| Function | Description |
|---|---|
| `os_get_proxy_wndproc()` | Returns a function pointer to an internal window-procedure trampoline (used by the GUI subsystem to route Win32 messages back into EZ callbacks) |

This is the FFI layer used internally by the GUI subsystem to talk to `user32.dll`/`gdi32.dll`, and is general enough to call most `*32.dll` Win32 APIs that take simple scalar/pointer arguments.

---

## 🖥 GUI Framework

The C++ runtime exposes roughly **70 raw `gui_*` native functions** (`src/GUIBuiltins.cpp`, ~1,200 lines) that wrap a Win32/GDI+ window, control, and drawing layer. These are the actual primitives available in this repository; a higher-level fluent OOP API (`gui.window(...).panel(...).button(...)`, as seen in example scripts via `use "lib/gui.ez"` or `use "ezgame"`) is an EZ-language wrapper that lives in the external `ezlib`/example scripts, not in this C++ source.



## 📦 Bundling Standalone Executables

EZ can package a script and all its `use`d dependencies into a single self-contained `.exe`:

```bash
ez bundle <entry_script.ez> [output.exe] [--gui] [--icon app.ico]
```

How it works:

1. The entry script is read and lexed; every `use "path"` statement is resolved (against `lib/`, `C:/ezlib/`, and literal paths) and the dependency graph is crawled recursively.
2. All discovered `.ez` files (plus the entry script as `__main__.ez`) are packed into an in-memory **virtual file system (VFS)** blob: `[fileCount][nameLen][name][contentLen][content]...`.
3. The current `ez.exe` binary is copied to the output path.
4. If `--icon app.ico` is given, the icon is injected into the `.exe`'s resources via `BeginUpdateResourceA`/`UpdateResourceA`/`EndUpdateResourceA` (parsing the `.ico` directory and rewriting it as a `GRPICONDIR` resource).
5. The VFS blob is appended to the end of the `.exe`, followed by a 4-byte size and the 6-byte magic marker `EZPKV1`.
6. If `--gui` is given, the PE header's `Subsystem` field is patched from console (3) to GUI (2) at byte offset `e_lfanew + 92`, hiding the console window when the bundled `.exe` runs.

At startup, `ez.exe` checks its own file for the trailing `EZPKV1` marker; if present, it loads the embedded VFS, and if a `__main__.ez` entry exists, runs it directly instead of starting the REPL or reading `argv[1]` as a script path.

---

## 🖥 REPL

Running `ez` with no arguments starts an interactive REPL:

```
EZ Language Interpreter v1.0 (Bytecode Mode)
Type 'exit' to quit
>>> x = 5
>>> out x * 2
10
>>> exit
Goodbye!
```

The REPL supports multi-line input — it counts `{`/`}` and keeps prompting with `...` until braces balance, then runs the type checker and bytecode compiler/VM on the accumulated input, sharing global state across REPL evaluations within the same session.

CLI summary (`ez --help`):

| Command | Description |
|---|---|
| `ez` | Run REPL (interactive mode) |
| `ez <file.ez> [--trace]` | Run a script file, optionally tracing bytecode execution |
| `ez install <pkg> [version]` | Install a package from the `ezlib` registry |
| `ez list` | List installed packages |
| `ez init <name>` | Scaffold a new package (`main.ez` + `package.ez`) |
| `ez bundle <file.ez> [out.exe] [--gui] [--icon app.ico]` | Create a standalone executable |
| `ez --help` / `-h` | Show usage |

---

## 📚 The `ezlib` Standard Library (external)

The [`ezlib`](https://github.com/imabd645/ezlib) repository is a separate registry of EZ-language packages installed with `ez install <name>` to `C:\ezlib\`. Packages such as `math`, `crypto`, `db`/`orm`, `pdf`, `fs`, `os`, `datetime`, `regex`, `collections`, `test`, `log`, `thread`, `gui`, `serve`, `http`, and `ai` — along with higher-level fluent wrappers like the `gui.window()...` chained API and the `game`/`ezgame` module used in the Snake/Flappy Bird/Fighter Jets demos — are implemented in **EZ itself** on top of the C++ runtime primitives described above (mainly the `os_*` FFI and `gui_*` raw builtins).

Because that repository is not part of this codebase, this README does **not** document its exact function signatures — refer to `ezlib` directly for that API surface. What *is* guaranteed by this repository is the underlying C++ runtime surface: the value types, operators, control flow, OOP system, type checker, the builtins listed in [Built-in Functions](#-built-in-functions-c-runtime), the FFI (`os_*`), and the raw GUI primitives (`gui_*`) that any `ezlib` package is ultimately built on.

---

## ⚠️ Known Gaps & Caveats

A summary of places where this repository's source diverges from commonly-circulated descriptions of EZ:

- **No `**` power operator** — the lexer does not produce a power token; use `pow(base, exp)`.
- **No `0b...` binary literals** — only decimal and `0x...` hex literals are lexed.
- **String interpolation uses `` `text {expr}` ``**, not `` `text ${expr}` ``.
- **No `db_*` (SQLite) or `pdf_*` builtins exist in C++** despite `sqlite3` being linked by CMake — any database or PDF functionality must come from an `ezlib` package.
- **No `md5`, `sha256`, or `hmac_sha256` builtins exist in C++** — general hashing lives in the `ezlib` `crypto` package. Base64 is partly covered: `b64url_encode`/`b64url_decode` (URL-safe alphabet) and `hex_to_bytes` are builtins, but standard-alphabet `base64_encode`/`base64_decode` are not.
- **No `listDir`, `getenv`, `setenv`, or `exec` builtins exist in C++.** File deletion, renaming and existence checks are available as statics on the `File` class (`File.remove`, `File.rename`, `File.exists`, `File.size`) rather than as free functions; directory listing and process control must come from an `ezlib` package or the FFI.
- **No `http_put`, `http_delete`, or `startServer`** C++ builtins — only `http_get`, `http_post`, and `fetch`.
- **Windows-only** — the code directly includes `<windows.h>` and Win32-specific structures (PE header patching, icon resources, `HMODULE`/`FARPROC` FFI), so it cannot be built on Linux/macOS as-is.
- **`match` arms support only equality comparison** against literal/expression patterns plus an `other` default — no ranges, guards, or destructuring.
- **Hard limits.** Call depth is capped at **4096** frames; exceeding it raises a catchable "maximum call depth exceeded" rather than crashing. A function may hold up to **65535** locals and capture up to **255** distinct outer variables. EZ frames live in a heap vector rather than on the C stack, so deep recursion does not consume native stack.
- **Interned strings are per-thread.** Strings of 14+ characters are deduplicated on the main thread only; worker threads allocate directly. This is a memory optimisation with no observable behavioural difference.
- **`other when` is the only valid else-if chain syntax** — consecutive bare `when` blocks are independent statements, not an if/else-if/else chain.

---

## 🤝 Contributing

### Reporting Issues

- Open an issue on [GitHub Issues](https://github.com/imabd645/EZ-language/issues)
- Include a minimal reproducing `.ez` script
- State your Windows version and compiler (MinGW / MSVC)

### Pull Requests

```bash
git clone https://github.com/imabd645/EZ-language.git
git checkout -b feature/your-feature
# make changes
git commit -m "feat: describe what you did"
git push origin feature/your-feature
# open a PR
```

### Code Style

- 4 spaces for indentation in C++ source
- Descriptive variable names — no single-letter identifiers except loop counters
- Comment any non-obvious logic
- If adding a new builtin, register it in the appropriate `Builtins_*.cpp` file via `interp.defineGlobal(...)` and ensure the file is included in both `CMakeLists.txt` and any manual build commands

---

## 📄 License

EZ Language is released under the [MIT License](LICENSE).

---

Built from scratch by **Abdullah Masood**

*A complete programming language: lexer → parser → optional type checker → bytecode compiler → stack VM → GC → native FFI/GUI layer*

[⭐ Star on GitHub](https://github.com/imabd645/EZ-language) · [🐛 Report a Bug](https://github.com/imabd645/EZ-language/issues)
