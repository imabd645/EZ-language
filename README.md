# EZ Language

EZ is a dynamic, interpreted programming language designed for simplicity and power. It features clean syntax, optional object-oriented programming, and built-in support for asynchronous tasks and web capabilities.

## 📖 Documentation

For more detailed documentation:
- [Language Specification](docs/specification.md)
- [API Reference](docs/api.md)
- [Tutorial](docs/tutorial.md)
- [FAQ](docs/faq.md)

---

## Table of Contents
1.  [Getting Started](#getting-started)
2.  [Syntax Basics](#syntax-basics)
3.  [Control Flow](#control-flow)
4.  [Functions (Tasks)](#functions-tasks)
5.  [Object-Oriented Programming](#object-oriented-programming)
6.  [Concurrency & Async](#concurrency--async)
7.  [Built-in Functions](#built-in-functions)

## Getting Started

To run an EZ script, use the interpreter:
```bash
./ez script.ez
```

### Comments
```javascript
# This is a line comment
/* This is a 
   block comment */
```

## Syntax Basics

### Variables
Variables are dynamically typed and declared by assignment.
```javascript
name = "EZ Language"
count = 42
isValid = true
```

### Data Types
- **Nil**: `nil`
- **Boolean**: `true`, `false`
- **Number**: Double-precision floating point (e.g., `10`, `3.14`).
- **String**: Strings with `'` or `"` quotes. Supports escape sequences (`\n`, `\t`).
- **Array**: Ordered lists.
    ```javascript
    items = [1, 2, "three", true]
    out items[0] # Access by index
    ```
- **Dictionary**: Key-value pairs.
    ```javascript
    user = { "name": "Alice", "age": 30 }
    out user["name"]
    user["city"] = "NY"
    ```

### Output
Use the `out` statement to print line to console.
```javascript
out "Hello World"
out 10 + 20
```

## Control Flow

### Conditionals
```javascript
when x > 10 {
    out "Large"
} other when x > 5 {
    out "Medium"
} other {
    out "Small"
}
```

### Loops
```javascript
count = 0
while count < 5 {
    out count
    count += 1
}

repeat i = 1 to 5 {
    out "Iteration " + str(i)
}

get item in [10, 20, 30] {
    out item
}
```

## Functions (Tasks)
Functions are defined using the `task` keyword. Values are returned using `give`.

```javascript
task add(a, b) {
    result = a + b
    give result
}
```

## Object-Oriented Programming
```javascript
model Dog extends Animal {
    init(name) {
        self.name = name
    }
    
    task speak() {
        out self.name + " barks!"
    }
}
```

## Concurrency & Async

### Multithreading
```javascript
task heavyWork(id) {
    stop(1000)
    give id
}
future = spawn(heavyWork, 1)
result = await(future)
```

### HTTP Fetch
```javascript
fut = fetch("https://api.example.com/data")
data = await(fut)
```

## Built-in Functions

*See [API Reference](docs/api.md) for full list.*
- `str`, `num`, `type`, `len`
- `push`, `pop`, `keys`, `values`
- `fetch`, `spawn`, `await`
- `clock`, `stop`, `rand`