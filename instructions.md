# EZ Language Guide

Welcome to the official guide for the EZ programming language. This document provides a comprehensive overview of the language features, syntax, and built-in functions.

## table of Contents

1. [Basic Syntax](#basic-syntax)
2. [Data Types](#data-types)
3. [Control Flow](#control-flow)
4. [Tasks (Functions)](#tasks)
5. [Models (Objects)](#models)
6. [Collections](#collections)
7. [Package Management](#package-management)
8. [Built-in Functions](#built-in-functions)
9. [Error Handling](#error-handling)

---

## 1. Basic Syntax

### Comments
```ez
// This is a single-line comment
```

### Variables
Variables are dynamically typed and can be declared anywhere.
```ez
name = "EZ Language"
version = 2.0
is_cool = yes
```

### Output
Use `out` to print to the console.
```ez
out "Hello, " + name
```

---

## 2. Data Types

- **Number**: 64-bit floating point (e.g., `10`, `3.14`)
- **String**: UTF-8 strings (e.g., `"hello"`)
- **Boolean**: `yes` / `no` / `true` / `false`
- **Nil**: Represents no value (`nil`)
- **Array**: Ordered list of values (`[1, 2, 3]`)
- **Dictionary**: Key-value pairs (`{name: "Alice", age: 30}`)

---

## 3. Control Flow

### `when` (Conditions)
```ez
when age >= 18 {
    out "Adult"
} or age >= 13 {
    out "Teen"
} other {
    out "Child"
}
```

### `repeat` (Iteration Loops)
The `repeat` loop is inclusive and supports both upward and downward ranges.
```ez
repeat i = 1 to 5 {
    out i // 1, 2, 3, 4, 5
}

repeat i = 5 to 1 {
    out i // 5, 4, 3, 2, 1
}
```

### `get` (Iteration)
Iterate over arrays, strings, or dictionaries.
```ez
colors = ["red", "green", "blue"]
get color in colors {
    out color
}

// Dictionaries iterate over keys
d = {a: 1, b: 2}
get k in d {
    out k + ": " + str(d[k])
}
```

### `while` (Condition Loops)
```ez
count = 0
while count < 5 {
    count = count + 1
}
```

---

## 4. Tasks (Functions)

Standalone functions are called `tasks`.
```ez
task greet(name) {
    give "Hello " + name
}

msg = greet("World")
```

---

## 5. Models (Objects)

EZ uses a modern `model` system for object-oriented programming.

```ez
model User {
    init(name, email) {
        self.name = name
        self.email = email
    }

    task printInfo() {
        out self.name + " (" + self.email + ")"
    }
}

u = User("Bob", "bob@example.com")
u.printInfo()
```

### Key Features:
- **`init`**: The constructor method.
- **`self`**: Reference to the current instance.
- **`task`**: Defines methods within the model.

---

## 6. Collections

### Arrays
```ez
arr = []
push(arr, "first")
val = pop(arr)
size = len(arr)
```

### Dictionaries
```ez
dict = {id: 1}
dict.name = "Test"
dict["type"] = "Internal"

exists = "name" in dict
dictRemove(dict, "name")
```

---

## 7. Package Management

EZ has a built-in package manager that resolves dependencies from GitHub.

### CLI Usage:
```bash
ez install math
ez install collections
```

### Script Usage:
```ez
use "math"
use "datetime"

db = use "database"
```

---

## 8. Built-in Functions

### String/Array Utilities
- `len(x)`: Returns length of string, array, or dictionary.
- `str(x)`: Converts value to string.
- `num(x)`: Converts to number.
- `split(str, delim)`: Returns array.
- `join(arr, delim)`: Returns string.
- `substr(str, start, len)`: Returns substring.

### Math Utilities
- `abs(x)`, `sqrt(x)`, `pow(b, e)`, `floor(x)`, `ceil(x)`, `round(x)`
- `rand()`: Random 0-1.
- `randint(min, max)`: Random integer.

### Database (SQLite)
- `dbOpen(path)`: Returns handle.
- `dbQuery(handle, sql, [params])`: Returns array of rows.
- `dbExec(handle, sql, [params])`: Executes command.
- `dbBegin(handle)`, `dbCommit(handle)`, `dbRollback(handle)`

---

## 9. Error Handling

Use `try` / `catch` to handle runtime errors.

```ez
try {
    x = 10 / 0
} catch err {
    out "Caught error: " + err
}
```
