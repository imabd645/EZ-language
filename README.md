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

[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg?style=flat-square)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-lightgrey.svg?style=flat-square)](https://github.com/imabd645/EZ-language)
[![Language](https://img.shields.io/badge/written%20in-C%2B%2B17-orange.svg?style=flat-square)](https://github.com/imabd645/EZ-language)
[![Packages](https://img.shields.io/badge/packages-19-green.svg?style=flat-square)](https://github.com/imabd645/ezlib)
[![Version](https://img.shields.io/badge/version-1.0-purple.svg?style=flat-square)](https://github.com/imabd645/EZ-language)

[Quick Start](#-quick-start) · [Syntax Guide](#-language-syntax) · [Built-ins](#-built-in-functions) · [OOP](#-object-oriented-programming) · [Async](#-async--concurrency) · [Packages](#-package-manager) · [Standard Library](#-standard-library)

</div>

---

## What is EZ?

EZ is a scripting language built from scratch  bytecode compiler, stack VM, garbage collector, and a full standard library ecosystem — all written in C++17. It replaces cryptic programming symbols with plain English keywords so code reads like what it actually does.

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

**Why EZ exists:** The goal is a language where beginners can focus on *thinking like a programmer*, not fighting with syntax. `when` instead of `if`, `task` instead of `function`, `give` instead of `return` — every keyword was chosen to be self-explanatory on first read.

---

## Table of Contents

- [Features](#-features)
- [Architecture](#-architecture)
- [Installation](#-installation)
- [Quick Start](#-quick-start)
- [Language Syntax](#-language-syntax)
  - [Variables & Types](#variables--types)
  - [Operators](#operators)
  - [Control Flow](#control-flow)
  - [Functions & Lambdas](#functions--lambdas)
  - [Arrays & Dictionaries](#arrays--dictionaries)
  - [Object-Oriented Programming](#-object-oriented-programming)
  - [Error Handling](#error-handling)
  - [Modules](#modules)
- [Built-in Functions](#-built-in-functions)
- [Async & Concurrency](#-async--concurrency)
- [Native FFI](#-native-ffi)
- [GUI Framework](#-gui-framework)
- [Package Manager](#-package-manager)
- [Standard Library](#-standard-library)
- [Roadmap](#-roadmap)
- [Contributing](#-contributing)

---

## ✨ Features

### Language Core
- **Natural English keywords** — `out`, `when`, `other`, `repeat`, `task`, `give`, `escape`, `skip`, `get`
- **Dynamic typing** with 18 runtime value types including a dedicated `INTEGER` path for fast integer arithmetic
- **Bytecode compilation** — source compiles to a custom stack-VM ISA before execution
- **Tail call optimization** — `give myFunc(...)` at tail position reuses the stack frame; 20,000-deep recursion without overflow
- **First-class closures** with Lua-style upvalues that migrate to the heap when their scope exits
- **Compile-time constant folding** — `3 + 4 * 2` evaluated at compile time, not runtime
- **String interpolation**, raw strings, ternary expressions (`cond ? a : b`), spread operator (`...arr`)

### Object-Oriented Programming
- **`model`** — classes with `init` constructors, `self` references, `hidden`/`shown` access modifiers
- **Single inheritance** via `extends` + `super` calls
- **`interface` / `implements`** with runtime method-presence validation
- **`static`** fields and methods on models
- **`struct`** for lightweight data holders
- **`toString()` override** — called automatically by `out` and string coercion

### Concurrency
- **Non-blocking Event Loop** — Single-threaded async execution for efficient I/O without OS thread overhead
- **`async task`** — Native coroutine that can be paused and resumed seamlessly via bytecode VM yielding
- **`await expr`** — Non-blocking yield until a future resolves, freeing the VM to run other tasks
- **`async { ... }` blocks** — Inline async expressions
- **`spawn(fn)`** — Detached background OS thread for heavy CPU work
- **First-class `Mutex()`** — `lock()` / `unlock()` as values
- **Atomic operations** — `atomicAdd`, `atomicGet`, `atomicSet`

### Runtime
- **Garbage collector** — cycle-detecting mark-sweep over a doubly-linked intrusive list, threshold-triggered (default 50,000 allocations)
- **Stack traces** — file, line, and call frame reported on runtime errors
- **Native FFI** — call any Windows DLL function directly: `os_load_lib("ws2_32.dll")` + `os_get_func(handle, "WSAStartup")`
- **Package manager** — `ez install <name>` downloads from the [ezlib registry](https://github.com/imabd645/ezlib)
- **Module system** — `use "path/to/file"` or `use "name" as alias` for namespaced imports

### Standard Library (19 packages, 6,664 lines of EZ)
Math · HTTP client · Web server · AI SDK · GUI · Testing · Collections · Regex · ORM · DateTime · Crypto · File system · OS · Logging · PDF · Threading · and more

---

## 🏗 Architecture

```
source.ez
    │
    ▼  Lexer.cpp  (565 lines, 37 token types)
Token stream
    │
    ▼  Parser.cpp  (1,136 lines, recursive descent)
AST  (std::variant, 17 expression + 18 statement node types)
    │
    ▼  BytecodeCompiler.cpp  (1,598 lines)
       · Upvalue resolution   · Constant folding
       · TCO detection        · Scope management
Bytecode chunks  (~80 opcodes, stack-VM ISA)
    │
    ▼  BytecodeVM.cpp  (1,954 lines)
       · Closure capture      · Async/Future dispatch
       · Exception handling   · Interface validation
Runtime values  (std::variant, 18 types, O(1) type lookup)
    │
    ▼  GarbageCollector  (mark-sweep, intrusive linked list)
```

**Total C++ source:** ~12,600 lines across 20 files, targeting Windows x64 with C++17.

---

## 🚀 Installation

### Prerequisites

| Dependency | Purpose |
|---|---|
| C++17 compiler (MinGW-w64 / MSVC) | Building the interpreter |
| CMake 3.10+ | Build system (optional) |
| libcurl | HTTP client/server |
| libsqlite3 | Database builtins |
| Win32 SDK | GUI, FFI, threading |

### Build from Source

```bash
git clone https://github.com/imabd645/EZ-language.git
cd EZ-language

# Using CMake
mkdir build && cd build
cmake .. -G "MinGW Makefiles"
cmake --build .

# Or direct g++ (MinGW)
g++ -std=c++17 -O2 -o ez.exe \
    src/main.cpp src/Lexer.cpp src/Parser.cpp \
    src/Bytecode.cpp src/BytecodeCompiler.cpp \
    src/BytecodeVM.cpp src/BytecodeInterpreter.cpp \
    src/Builtins.cpp src/GUIBuiltins.cpp \
    src/GC.cpp src/GCObject.cpp \
    src/runtime/Builtins_IO.cpp src/runtime/Builtins_Math.cpp \
    src/runtime/Builtins_Net.cpp src/runtime/Builtins_String.cpp \
    src/runtime/Builtins_Data.cpp src/runtime/Builtins_Sys.cpp \
    src/runtime/Builtins_Buffer.cpp src/runtime/Builtins_Concurrency.cpp \
    src/runtime/Builtins_PDF.cpp \
    -lsqlite3 -lcurl -lws2_32 -lpthread -ldwmapi -luxtheme \
    -I src
```

### Add to PATH

1. Move `ez.exe` to a permanent folder (e.g. `C:\ez\`)
2. Open **System Properties → Environment Variables → Path → Edit → New**
3. Add `C:\ez`
4. Restart your terminal

```bash
ez --version     # confirm it works
ez hello.ez      # run a script
ez              # start the REPL
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

# Output
out "Name: " + name
out "Age:  " + str(age)

# Conditional
when age >= 18 {
    out name + " is an adult"
} other {
    out name + " is a minor"
}

# Loop — repeat N to M (inclusive)
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

### Variables & Types

Variables are dynamically typed. No declaration keyword — just assign.

```ez
# Numbers — two internal types: double and long long
x     = 42          # INTEGER (long long)
pi    = 3.14159     # NUMBER  (double)
big   = 0xFF        # hex literal

# Strings
msg   = "Hello, EZ!"
raw   = r"C:\Users\file.txt"           # raw string — backslashes literal
interp = "Pi is approximately ${pi}"  # string interpolation

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

**Type names** (returned by `typeOf()`): `"number"`, `"integer"`, `"string"`, `"boolean"`, `"array"`, `"dictionary"`, `"function"`, `"nil"`, `"model"`, `"future"`, `"buffer"`, `"mutex"`

### Operators

```ez
# Arithmetic
out 10 + 3      # 13
out 10 - 3      # 7
out 10 * 3      # 30
out 10 / 3      # 3.333...
out 10 % 3      # 1  (modulo)
out 2 ** 8      # 256 (power)

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
out 0b1010 & 0b1100   # 8   (AND)
out 0b1010 | 0b0101   # 15  (OR)
out 0b1010 ^ 0b1100   # 6   (XOR)
out ~5                 # bitwise NOT
out 1 << 3             # 8   (left shift)
out 16 >> 2            # 4   (right shift)

# Ternary
label = age >= 18 ? "adult" : "minor"

# Spread
a = [1, 2, 3]
b = [...a, 4, 5]      # [1, 2, 3, 4, 5]
```

### Control Flow

```ez
# when / other when / other  (if / else if / else)
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

# while loop
n = 1
while n <= 10 {
    out str(n)
    n += 1
}

# repeat N to M  (for i = N; i <= M; i++)
repeat i = 1 to 100 {
    out str(i)
}

# get ... in  (for-each)
get item in ["alpha", "beta", "gamma"] {
    out item
}

# get ... in  (for key in dictionary)
config = {"host": "localhost", "port": 8080}
get key in config {
    out key + " = " + str(config[key])
}

# get [k, v] in  (for key, value in dictionary)
get [k, v] in config {
    out k + " = " + str(v)
}

# match statement (pattern matching / switch)
state = "success"
match state {
    "loading" => out "Please wait..."
    "success" => {
        out "Operation completed!"
    }
    other => out "Unknown state"
}

# escape (break) and skip (continue)
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

# Variadic functions
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

# Tail-call optimized recursion (20,000+ levels)
task count(n, acc) {
    when n == 0 { give acc }
    give count(n - 1, acc + 1)    # TCO — no stack overflow
}

# Lambda — single expression
double = |x| x * 2

# Lambda — multi-statement body
clamp = |val, lo, hi| {
    when val < lo { give lo }
    when val > hi { give hi }
    give val
}

# Higher-order functions
task applyTwice(f, x) {
    give f(f(x))
}
out str(applyTwice(double, 3))    # 12

# Closures
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

# Async task
async task fetchUser(id) {
    data = http_get("https://api.example.com/users/" + str(id))
    give data
}
future = fetchUser(1)
result = await future
```

### Arrays & Dictionaries

```ez
# Array literals
primes = [2, 3, 5, 7, 11]

# Indexing (0-based)
out str(primes[0])      # 2
out str(primes[-1])     # 11  (negative indexing from end)

# Array operations
push(primes, 13)
pop(primes)
out str(len(primes))    # 5

# Slice
out str(primes[1:3])    # [3, 5]

# Spread into function call
args = [3, 4]
out str(add(...args))

# Dictionary (hash map)
user = {
    "name": "Alice",
    "age":  30,
    "tags": ["admin", "user"]
}

out user["name"]                    # Alice
out str(user["age"])                # 30
out str(has_key(user, "email"))     # false

# Add / update
user["email"] = "alice@example.com"

# Iterate
get key in user {
    out key + ": " + str(user[key])
}

# Dictionary functions
out str(keys(user))     # ["name", "age", "tags", "email"]
out str(len(user))      # 4
dictRemove(user, "tags")
```

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
        give self    # enable method chaining
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

### Interfaces

```ez
interface Serializable {
    toJson
    fromJson
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

# Runtime validates that Config has toJson and fromJson
cfg = Config({"debug": true, "port": 3000})
out cfg.toJson()
```

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

---

### Error Handling

```ez
# try / catch / throw
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

# Throw any value — string, number, or model
model AppError {
    init(code, message) {
        self.code    = code
        self.message = message
    }
}

try {
    throw AppError(404, "Not found")
} catch e {
    when typeOf(e) == "model" {
        out "Error " + str(e.code) + ": " + e.message
    }
}

# Nested try-catch
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

### Modules

```ez
# Import entire file — all its globals become available
use "lib/utils.ez"

# Namespaced import — access via alias.name
use "lib/math.ez" as math
out str(math.sqrt(25))    # 5

# Import everything from a module into current scope
use "lib/helpers.ez" as *

# Install and use a package (after: ez install collections)
use "collections"
s = Set().add(1).add(2).add(1)
out str(s.size())    # 2
```

---

## 🔧 Built-in Functions

### Input / Output

| Function | Description |
|---|---|
| `out expr` | Print value with newline |
| `input(prompt)` | Read a line from stdin |
| `color(code)` | Set Windows console text color (0–255) |
| `reset()` | Reset console color |

### Type Conversion & Inspection

| Function | Description |
|---|---|
| `str(val)` | Convert any value to string |
| `num(val)` | Parse string to number |
| `int(val)` | Convert to integer |
| `bool(val)` | Convert to boolean |
| `typeOf(val)` | Return type name as string |
| `len(val)` | Length of string, array, or dictionary |

### String Functions

| Function | Description |
|---|---|
| `substr(s, start, len)` | Extract substring |
| `split(s, delim)` | Split string to array |
| `join(arr, delim)` | Join array to string |
| `upper(s)` / `lower(s)` | Case conversion |
| `trim(s)` | Strip leading/trailing whitespace |
| `replace(s, old, new)` | Replace substring |
| `contains(s, sub)` | Check if substring exists |
| `startsWith(s, prefix)` | Prefix check |
| `endsWith(s, suffix)` | Suffix check |
| `indexOf(s, sub)` | First index or -1 |
| `ord(char)` | Character to ASCII code |
| `chr(code)` | ASCII code to character |
| `repeat(s, n)` | Repeat string N times |
| `padLeft(s, w, char)` | Left-pad to width |
| `padRight(s, w, char)` | Right-pad to width |

### Array Functions

| Function | Description |
|---|---|
| `push(arr, val)` | Append to array |
| `pop(arr)` | Remove and return last element |
| `shift(arr)` | Remove and return first element |
| `unshift(arr, val)` | Prepend to array |
| `reverse(arr)` | Reverse in place |
| `sort(arr)` | Sort in place |
| `slice(arr, start, end)` | Return sub-array |
| `concat(a, b)` | Merge two arrays |
| `indexOf(arr, val)` | Find value index |
| `includes(arr, val)` | Boolean membership check |
| `filter(arr, fn)` | Return elements where fn returns true |
| `map(arr, fn)` | Transform each element |
| `reduce(arr, fn, init)` | Fold array to single value |
| `flat(arr)` | Flatten nested arrays |

### Dictionary Functions

| Function | Description |
|---|---|
| `keys(dict)` | Array of keys |
| `values(dict)` | Array of values |
| `has_key(dict, key)` | Boolean key check |
| `dictRemove(dict, key)` | Delete a key |
| `merge(a, b)` | Merge two dicts (b wins on conflict) |

### Math Functions

| Function | Description |
|---|---|
| `floor(x)` | Round down |
| `ceil(x)` | Round up |
| `round(x)` | Round to nearest integer |
| `abs(x)` | Absolute value |
| `sqrt(x)` | Square root |
| `pow(base, exp)` | Exponentiation |
| `min(a, b)` | Minimum of two values |
| `max(a, b)` | Maximum of two values |
| `rand()` | Random float [0, 1) |
| `randint(lo, hi)` | Random integer in range |
| `xor(a, b)` | Bitwise XOR |
| `pi` | 3.141592653589793 |

### File I/O

| Function | Description |
|---|---|
| `readFile(path)` | Read entire file as string |
| `writeFile(path, content)` | Write/overwrite file |
| `appendFile(path, content)` | Append to file |
| `readLines(path)` | Read file as array of lines |
| `writeLine(path, line)` | Write single line |
| `deleteFile(path)` | Delete a file |
| `listDir(path)` | List directory contents |
| `fs_exists(path)` | Check if path exists |

### JSON

| Function | Description |
|---|---|
| `parse_json(str)` | Parse JSON string to EZ value |
| `to_json(val)` | Serialize EZ value to JSON string |

### Database (SQLite)

| Function | Description |
|---|---|
| `db_open(path)` | Open/create SQLite database, returns handle |
| `db_execute(handle, sql)` | Run non-returning SQL |
| `db_query(handle, sql)` | Run SELECT, returns array of dicts |
| `db_last_insert_id(handle)` | Last auto-increment ID |
| `db_begin(handle)` | Begin transaction |
| `db_commit(handle)` | Commit transaction |
| `db_rollback(handle)` | Rollback transaction |
| `db_close(handle)` | Close database |

### HTTP (via libcurl)

| Function | Description |
|---|---|
| `http_get(url, headers?)` | HTTP GET |
| `http_post(url, body, headers?)` | HTTP POST |
| `http_put(url, body, headers?)` | HTTP PUT |
| `http_delete(url, headers?)` | HTTP DELETE |
| `startServer(port, handler)` | Start HTTP server |

### Regular Expressions

| Function | Description |
|---|---|
| `reMatch(text, pattern)` | Test if text matches pattern |
| `reSearch(text, pattern)` | Find first match, return groups array |
| `reReplace(text, pattern, repl)` | Replace first match |

### Crypto & Encoding

| Function | Description |
|---|---|
| `md5(str)` | MD5 hex digest |
| `sha256(str)` | SHA-256 hex digest |
| `base64_encode(str)` | Base64 encode |
| `base64_decode(str)` | Base64 decode |
| `hmac_sha256(key, msg)` | HMAC-SHA256 hex digest |

### Time & System

| Function | Description |
|---|---|
| `clock()` | Milliseconds since process start |
| `wait(ms)` | Sleep for N milliseconds |
| `exit(code)` | Exit process |
| `exec(cmd)` | Execute shell command, return output |
| `getenv(name)` | Read environment variable |
| `setenv(name, val)` | Set environment variable |

### Concurrency

| Function | Description |
|---|---|
| `spawn(fn, ...args)` | Start detached thread |
| `await future` | Block until future resolves |
| `mutex()` | Create a `Mutex` value |
| `lock(mu, fn)` | Acquire mutex, run fn, release |
| `atomicAdd(ref, n)` | Atomic integer add |
| `atomicGet(ref)` | Atomic integer read |
| `atomicSet(ref, n)` | Atomic integer write |

### Buffers

| Function | Description |
|---|---|
| `Buffer(size)` | Allocate raw byte buffer |
| `buf.readU8(offset)` | Read unsigned byte |
| `buf.writeU8(offset, val)` | Write unsigned byte |
| `buf.readU32(offset)` | Read 32-bit unsigned int |
| `buf.writeU32(offset, val)` | Write 32-bit unsigned int |
| `buf.readString(offset, len)` | Read UTF-8 string |
| `buf.writeString(offset, str)` | Write UTF-8 string |

### Metaprogramming

| Function | Description |
|---|---|
| `getattr(obj, name)` | Get property by name string |
| `setattr(obj, name, val)` | Set property by name string |
| `hasattr(obj, name)` | Check if property exists |
| `typeOf(val)` | Runtime type name |

---

## ⚡ Async & Concurrency

EZ's concurrency model is built on a custom native Event Loop, exposing true non-blocking I/O with clean syntax without the overhead of spawning OS threads per task.

```ez
# Async task — runs concurrently on the Event Loop
async task fetchJson(url) {
    raw = http_get(url)
    give parse_json(raw)
}

# Launch two fetches in parallel
f1 = fetchJson("https://api.example.com/users")
f2 = fetchJson("https://api.example.com/posts")

out "Fetching in parallel..."

# Await both — total time ≈ max(t1, t2), not t1 + t2
users = await f1
posts = await f2

out "Got " + str(len(users)) + " users"
out "Got " + str(len(posts)) + " posts"

# Async block expression
result = await async {
    wait(500)
    give "done"
}

# Inline await
out await fetchJson("https://api.example.com/status")
```

### Thread-Safe Shared State

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

# Spawn 10 threads all incrementing
futures = []
repeat i = 1 to 10 {
    f = async counter.increment()
    push(futures, f)
}

# Wait for all
get f in futures { await f }
out str(counter.get())    # 10
```

---

## 🔗 Native FFI

EZ can call any exported function from any Windows DLL without writing C++.

```ez
# Load a DLL
kernel = os_load_lib("kernel32.dll")

# Get a function pointer
sleep_fn = os_get_func(kernel, "Sleep")

# Call it (arguments passed as EZ values, auto-marshalled)
sleep_fn(1000)    # sleep 1 second via Win32

# Example: call MessageBox
user32 = os_load_lib("user32.dll")
msgbox = os_get_func(user32, "MessageBoxA")
msgbox(0, "Hello from EZ!", "EZ FFI Demo", 0)
```

This is how `ezserve` implements its web server — by directly calling `ws2_32.dll` (Winsock) from EZ code with zero C++ required.

---

## 🖥 GUI Framework

EZ includes a native Win32/GDI+ GUI framework with a fluent OOP API.

```ez
use "gui"

gui.setTheme("dark")    # Immersive dark mode (including title bar)

win = gui.window("My App", 900, 600)

# Panels — containers for grouping widgets
sidebar = win.panel(0, 0, 200, 600).color("#1e1e1e")
content = win.panel(200, 0, 700, 600)

# Widgets
sidebar.label("Navigation", 20, 20, 160, 30).font("Segoe UI", 16)
sidebar.button("Home",     20,  60, 160, 36, || { showHome() })
sidebar.button("Settings", 20, 106, 160, 36, || { showSettings() })
sidebar.button("About",    20, 152, 160, 36, || { showAbout() })

title = content.label("Welcome", 30, 30, 400, 48).font("Segoe UI", 28)

input = content.input("", 30, 100, 400, 36)
content.button("Submit", 30, 150, 120, 36, || {
    val = input.getText()
    out "Submitted: " + val
})

# Tabs
tabs = win.tabs(200, 0, 700, 600)
tabs.addTab("Dashboard")
tabs.addTab("Reports")
tabs.addTab("Users")

# Dropdown
dropdown = content.dropdown(30, 200, 200, 36, ["Option 1", "Option 2", "Option 3"])
dropdown.onChange(|val| { out "Selected: " + val })

# Canvas drawing
canvas = content.canvas(30, 250, 400, 200)
canvas.fillRect(0, 0, 400, 200, "#1a1a2e")
canvas.drawCircle(200, 100, 60, "#e94560")
canvas.drawText("EZ Canvas", 140, 95, "#ffffff", 18)

# Scroll area
scroll = content.scrollPanel(30, 30, 640, 500)
repeat i = 1 to 50 {
    scroll.label("Item " + str(i), 10, (i - 1) * 30, 200, 28)
}

win.run()    # blocks on event loop
```

### Available Widget Types

| Widget | Method | Description |
|---|---|---|
| Window | `gui.window(title, w, h)` | Main application window |
| Panel | `win.panel(x, y, w, h)` | Container / layout group |
| Label | `.label(text, x, y, w, h)` | Static text |
| Button | `.button(text, x, y, w, h, fn)` | Clickable button |
| Input | `.input(placeholder, x, y, w, h)` | Single-line text field |
| Checkbox | `.checkbox(label, x, y, w, h, fn)` | Toggle checkbox |
| Slider | `.slider(x, y, w, h, min, max, fn)` | Range slider |
| Dropdown | `.dropdown(x, y, w, h, options)` | Select list |
| Tabs | `.tabs(x, y, w, h)` | Tab strip navigation |
| ScrollPanel | `.scrollPanel(x, y, w, h)` | Scrollable container |
| Canvas | `.canvas(x, y, w, h)` | 2D drawing surface |
| ProgressBar | `.progressBar(x, y, w, h)` | Progress indicator |

---

## 📦 Package Manager

EZ comes with a built-in package manager that uses [ezlib](https://github.com/imabd645/ezlib) as its registry — a GitHub monorepo where packages are ZIP-downloaded and installed to `C:\ezlib`.

### Commands

```bash
ez install <package>        # install a package
ez install math             # install ezmath
ez install ai               # install ezai (OpenAI, Claude, Gemini, DeepSeek)
ez install collections      # install ezcollections
ez update <package>         # update to latest version
ez remove <package>         # uninstall a package
ez list                     # show installed packages
ez search <term>            # search the registry
```

### How It Works

```
ez install math
    │
    ├─ Fetch index.json from github.com/imabd645/ezlib
    ├─ Resolve latest version (semver: ^, ~, >=)
    ├─ Download ZIP archive via curl
    ├─ Extract to C:\ezlib\math\
    ├─ Read package.ez manifest
    ├─ Recursively install dependencies
    └─ Write C:\ezlib\packages.json
```

### Using a Package

```ez
# After: ez install math
use "math"

out str(math.isPrime(97))          # true
out str(math.fibonacci(10))        # 55

v = Vector2(3, 4)
out str(v.length())                # 5.0

s = stats([2, 4, 4, 4, 5, 5, 7, 9])
out str(s.mean())                  # 5.0
out str(s.stddev())                # 2.0
```

### Creating a Package

```bash
ez init mypackage
```

This generates:

```
mypackage/
├── main.ez          # entry point
└── package.ez       # manifest
```

`package.ez`:
```json
{
  "name": "mypackage",
  "version": "1.0.0",
  "description": "A short description",
  "author": "your-github-username",
  "main": "main.ez",
  "license": "MIT",
  "dependencies": {}
}
```

Push to GitHub as `imabd645/ez-mypackage` and anyone can install it with `ez install mypackage`.

---

## 📚 Standard Library

The [ezlib](https://github.com/imabd645/ezlib) registry contains 19 official packages totalling 6,664 lines of EZ code.

### `math` — Mathematics

```ez
use "math"

# Constants
out str(math.PI)       # 3.141592653589793
out str(math.PHI)      # 1.618033988749895  (golden ratio)

# Trigonometry (degrees and radians)
out str(math.sind(45))                  # 0.7071...
out str(math.atan2d(1, 1))             # 45.0

# Number theory
out str(math.isPrime(97))              # true
out str(math.gcd(48, 18))             # 6
out str(math.fibonacci(10))            # 55

# Statistics model
s = stats([10, 20, 30, 40, 50])
out str(s.mean())                      # 30.0
out str(s.stddev())                    # 14.142...
out str(s.percentile(75))             # 40.0
q = s.quartiles()
out str(q["iqr"])                      # 20.0

# Vectors
v1 = vec2(3, 4)
v2 = vec2(1, 0)
out str(v1.length())                   # 5.0
out str(v1.dot(v2))                   # 3.0

v3d = vec3(1, 2, 3)
out str(v3d.normalize().length())     # 1.0

# Random
out str(math.randInt(1, 6))          # 1–6 dice roll
out str(math.randNormal(0, 1))       # standard normal sample

# Interpolation & easing
out str(math.lerp(0, 100, 0.75))     # 75.0
out str(math.easeInOutCubic(0.5))    # 0.5
```

### `test` — Testing Framework

```ez
use "test"

describe("String utilities", || {
    beforeEach(|| {
        # setup runs before each test
    })

    it("trims whitespace", || {
        assertEqual(trim("  hello  "), "hello")
    })

    it("splits on delimiter", || {
        parts = split("a,b,c", ",")
        assertDeepEqual(parts, ["a", "b", "c"])
    })

    it("detects substrings", || {
        assert(contains("Hello World", "World"))
    })
})

describe("Error handling", || {
    it("throws on bad input", || {
        assertThrows(|| {
            divide(10, 0)
        }, "Division by zero")
    })
})

runTests()
```

**Assertion functions:** `assert`, `assertFalse`, `assertEqual`, `assertDeepEqual`, `assertNotEqual`, `assertNull`, `assertNotNull`, `assertType`, `assertContains`, `assertInArray`, `assertThrows`, `assertDoesNotThrow`, `assertApproxEqual`, `assertGreaterThan`, `assertLessThan`

**Hooks:** `beforeEach`, `afterEach`, `beforeAll`, `afterAll`

**Mocking:** `spy(fn)`, `stub(returnValue)`, `mock()`

### `http` — HTTP Client

```ez
use "http"

# GET
response = HTTPRequest("GET", "https://api.github.com/users/imabd645")
    .header("User-Agent", "EZ/1.0")
    .send()

when response.isOk() {
    user = response.json()
    out user["login"] + " has " + str(user["public_repos"]) + " repos"
}

# POST with JSON body — auto-serialized
res = HTTPRequest("POST", "https://api.example.com/items")
    .data({"name": "Widget", "price": 9.99})
    .timeout(5000)
    .retries(3)
    .send()

res.raiseForStatus()    # throws if 4xx/5xx
out res.json()
```

### `serve` — Web Server

```ez
use "serve"

app = App()

app.get("/", |req| {
    give {"status": 200, "body": "<h1>Hello from EZ!</h1>",
          "headers": {"Content-Type": "text/html"}}
})

app.get("/api/health", |req| {
    give {"status": 200, "body": to_json({"ok": true, "time": clock()})}
})

app.post("/api/echo", |req| {
    data = parse_json(req["body"])
    give {"status": 200, "body": to_json(data)}
})

# Serve static files from ./public/
app.serveStatic("/static", "public")

out "Listening on port 8080"
app.listen(8080)
```

### `ai` — Unified AI SDK

```ez
use "ai"

# Auto-detect provider from API key prefix
response = ask("Explain closures in one sentence", myApiKey)
out response

# Explicit provider
reply = anthropic("What is a monad?", claudeApiKey)
reply = openai("Write a haiku about recursion", openaiKey)
reply = gemini("Summarize quantum computing", geminiKey)
reply = deepseek("Translate to French: hello world", deepseekKey)

# Specific model
reply = anthropic_model("Solve x^2 - 5x + 6 = 0", key, "claude-opus-4-6")

# Multi-turn chat session
chat = new_chat(myApiKey)
chat_send(chat, "My name is Alice.")
reply = chat_send(chat, "What is my name?")
out reply    # Alice

# With system prompt
chat = new_chat_with_system(key, "You are a helpful math tutor.")
reply = chat_send(chat, "Explain the Pythagorean theorem.")
```

### `collections` — Data Structures

```ez
use "collections"

# Set — unique values, any type
s = Set()
s.add(1).add("two").add(true).add(1)   # duplicate 1 ignored
out str(s.size())                       # 3
out str(s.has("two"))                  # true
s.remove(1)
out str(s.toArray())

# Map — typed key-value store
m = Map()
m.set(42, "answer").set("pi", 3.14)
out str(m.get(42))                     # answer
out str(m.has("missing"))             # false

# Queue — FIFO
q = Queue()
q.enqueue("first").enqueue("second").enqueue("third")
out q.dequeue()                        # first
out q.peek()                           # second

# Stack — LIFO
st = Stack()
st.push(10).push(20).push(30)
out str(st.pop())                      # 30
out str(st.peek())                     # 20

# Counter — frequency map
c = Counter(["a", "b", "a", "c", "a", "b"])
out str(c.count("a"))                  # 3
out str(c.mostCommon(2))              # [["a", 3], ["b", 2]]
```

### `datetime` — Date & Time

```ez
use "datetime"

# Current timestamp
ts = now()
out format(ts)                              # 2026-01-15 14:32:08

# Components
comp = getComponents(ts)
out str(comp["year"]) + "-" + str(comp["month"])

# Arithmetic
tomorrow = addTime(ts, 1, 0, 0, 0)         # +1 day
nextHour = addTime(ts, 0, 1, 0, 0)         # +1 hour
out str(diffDays(tomorrow, ts))            # 1

# Relative formatting
old = addTime(ts, -3, 0, 0, 0)
out timeAgo(old)                           # "3 days ago"
```

### `db` — Database (SQLite)

```ez
use "database"

db = Database("app.db")

# Define table with schema dict
users = db.define("users", {
    "name":  "TEXT NOT NULL",
    "email": "TEXT UNIQUE",
    "score": "INTEGER DEFAULT 0"
})

# Fluent query builder
results = db.query("SELECT * FROM users").where("score > 50").orderBy("score DESC").limit(10).run()

# Transactions
db.begin()
try {
    db.execute("INSERT INTO users (name, email) VALUES ('Alice', 'alice@example.com')")
    db.execute("UPDATE users SET score = 100 WHERE name = 'Alice'")
    db.commit()
} catch e {
    db.rollback()
    out "Transaction failed: " + str(e)
}
db.close()
```

### `orm` — Object-Relational Mapper

```ez
use "orm"

db = Database("shop.db")

Product = db.define("products", {
    "name":     "TEXT NOT NULL",
    "price":    "REAL",
    "category": "TEXT"
})

# CRUD
id = Product.insert({"name": "Widget", "price": 9.99, "category": "tools"})
product = Product.find(id)
out product["name"]                              # Widget

all = Product.findAll()
widgets = Product.findWhere("category = 'tools'")

Product.update(id, {"price": 7.99})
Product.delete(id)
```

### `regex` — Regular Expressions

```ez
use "regex"

# OOP interface
email = Regex(r"^[\w.+-]+@[\w-]+\.[a-z]{2,}$")
out str(email.match("user@example.com"))    # true
out str(email.match("not-an-email"))        # false

# Find all matches
ip = Regex(r"\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}")
results = ip.findAll("Servers: 192.168.1.1 and 10.0.0.2")
out str(len(results))                        # 2

# Replace
cleaned = Regex(r"\s+").replaceAll("too   much    space", " ")
out cleaned    # "too much space"

# Pre-built patterns
out str(reEmail.match("alice@example.com"))  # true
out str(reUrl.match("https://github.com"))  # true
out str(reIPv4.match("192.168.1.1"))        # true
out str(rePhone.match("+1-555-0100"))        # true
```

### `log` — Structured Logger

```ez
use "log"

log.setLevel(DEBUG)              # DEBUG / INFO / WARN / ERROR / FATAL
log.setFile("app.log")           # also write to file

log.debug("Processing item 42")
log.info("Server started on :8080")
log.warn("Cache miss rate above 80%")
log.error("Failed to connect to database")
log.fatal("Unrecoverable state — shutting down")

# Custom logger instance
myLog = Logger()
myLog.setLevel(WARN)
myLog.enableConsole(true)
myLog.warn("This won't go to default log")
```

### `fs` — File System

```ez
use "fs"

# Read / write
content = fs.readFile("data.txt")
fs.writeFile("output.txt", "hello\n")
fs.appendFile("log.txt", "entry\n")

# Directory
files = fs.listDir("./src", ".ez")      # filter by extension
when not fs.exists("output/") {
    fs.mkdir("output/")
}

# Copy / move
fs.copyFile("src.txt", "dst.txt")
fs.moveFile("old.txt", "new.txt")

# Path helpers
out fs.getExtension("report.pdf")      # .pdf
out fs.getBasename("/usr/local/ez.exe") # ez.exe
```

### `os` — Operating System

```ez
use "os"

# Environment
home = os.getEnv("USERPROFILE")
os.setEnv("MY_VAR", "hello")

# Process execution
output = os.exec("git log --oneline -5")
out output

# System info
info = os.info()
out info["os"]      # Windows
out info["arch"]    # x64

# Clipboard
os.clipboard.set("Copied from EZ!")
text = os.clipboard.get()

# Notifications
os.notify("EZ App", "Build completed successfully")

# File dialog
path = os.dialog.openFile("Open Script", "EZ Files (*.ez)\0*.ez\0")
```

### `thread` — High-Level Threading

```ez
use "thread"

# Parallel map — fan out + collect
results = thread.parallelMap([1, 2, 3, 4, 5], |n| {
    wait(100)       # simulate I/O
    give n * n
})
out str(results)    # [1, 4, 9, 16, 25]

# Await all futures
f1 = async expensiveComputation(data1)
f2 = async expensiveComputation(data2)
f3 = async expensiveComputation(data3)
all_results = thread.all([f1, f2, f3])

# Task group
group = thread.createGroup()
group.add(fetchUser, 1)
group.add(fetchUser, 2)
group.add(fetchUser, 3)
users = group.wait()

# Sleep
thread.sleep(500)   # 500ms
```

### `crypto` — Cryptography

```ez
use "crypto"

# Hashing
out sha256("hello world")
out md5("quick check")       # not for security use

# HMAC
tag = hmac_sha256("secret-key", "message body")
out tag

# Encoding
encoded = base64_encode("Binary data: \x00\x01\x02")
decoded = base64_decode(encoded)

# Ciphers (educational)
ciphertext = xorCipher("Hello, EZ!", "mykey")
plaintext  = xorCipher(ciphertext, "mykey")   # XOR is its own inverse
```

### `gui` — Native GUI Framework

> See [GUI Framework](#-gui-framework) section above for full documentation.

### Package summary

| Package | Install | Lines | Highlights |
|---|---|---|---|
| `math` | `ez install math` | 778 | Taylor trig, Box-Muller RNG, Statistics, Vector2/3 |
| `test` | `ez install test` | 749 | Jest-style framework, 15 assertions, Spy/mock |
| `http` | `ez install http` | 558 | Fluent builder, retries, auto-JSON, status constants |
| `gui` | `ez install gui` | 806 | Win32 widgets, dark mode, canvas, tabs |
| `serve` | `ez install serve` | 388 | Raw Winsock FFI, backpressure, MIME routing |
| `ai` | `ez install ai` | 442 | OpenAI, Claude, Gemini, DeepSeek, multi-turn chat |
| `collections` | `ez install collections` | 226 | Set, Map, Queue, Stack, LinkedList, Counter |
| `regex` | `ez install regex` | 482 | Regex model, findAll, named groups, common patterns |
| `pm` | `ez install pm` | 758 | Semver, ez.lock, remote index, recursive deps |
| `orm` | `ez install orm` | 179 | Active-record CRUD over SQLite |
| `db` | `ez install database` | 238 | Query builder, transactions, SQLite wrapper |
| `datetime` | `ez install datetime` | 203 | Timestamps, formatting, arithmetic, relative time |
| `crypto` | `ez install crypto` | 215 | SHA-256, HMAC, Base64, XOR/Caesar ciphers |
| `fs` | `ez install fs` | 130 | Read/write/copy/move/list with path helpers |
| `log` | `ez install log` | 102 | 5-level logger, color output, file appending |
| `os` | `ez install os` | 193 | Env, exec, clipboard, dialogs, notifications |
| `thread` | `ez install thread` | 63 | parallelMap, all(), TaskGroup |
| `pdf` | `ez install pdf` | 61 | OOP wrapper over native pdf_* builtins |
| `game` | `ez install game` | 93 | Delta-time game loop scaffold over GUI |

---

## 🗺 Roadmap

| Milestone | Status |
|---|---|
| Bytecode VM + TCO | ✅ Done |
| Closures + upvalues | ✅ Done |
| Async/await + futures | ✅ Done |
| OOP: models, interfaces, static | ✅ Done |
| Native FFI (DLL calls from EZ) | ✅ Done |
| Package manager v2 (semver, lock file) | ✅ Done |
| GUI framework (Win32) | ✅ Done |
| Linux / macOS support | 🔜 Planned |
| Prepared statements (SQL injection fix) | 🔜 Planned |
| Real-time timestamps in `clock()` | 🔜 Planned |
| HTTPS / TLS in `serve` | 🔜 Planned |
| REPL with persistent state | 🔜 Planned |
| VSCode syntax extension | 🔜 Planned |

---

## 🤝 Contributing

Contributions are welcome. Here's how to get involved:

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
- If adding a new builtin, register it in the appropriate `Builtins_*.cpp` file and add it to `CMakeLists.txt`

### Adding a Standard Library Package

1. Fork [ezlib](https://github.com/imabd645/ezlib)
2. Create a folder `ez<name>/` with `main.ez` and `package.ez`
3. Add your package entry to `index.json`
4. Open a PR — once merged, `ez install <name>` works for everyone

---

## 📄 License

EZ Language and ezlib are released under the [MIT License](LICENSE).

---


Built from scratch by **Abdullah Masood** 

*A complete programming language: lexer → parser → bytecode compiler → stack VM → GC → standard library*

[⭐ Star on GitHub](https://github.com/imabd645/EZ-language) · [📦 ezlib packages](https://github.com/imabd645/ezlib) · [🐛 Report a Bug](https://github.com/imabd645/EZ-language/issues)

