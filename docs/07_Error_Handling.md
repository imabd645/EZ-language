# Exception Handling

EZ uses a modern exception bubbling system to handle runtime anomalies, preventing silent failures.

## 1. Try / Catch
Use `try` to sandbox code that might fail (e.g., File I/O, Networking, invalid calculations). If an error occurs, execution jumps immediately to the `catch` block.

```ez
task readConfig() {
    try {
        content = readFile("config.json")
        out "Config loaded!"
        give content
    } catch err {
        // This block executes if an exception is thrown
        out "Failed to load config. Generating default..."
        out "Reason: " + err
        give "{ \"host\": \"localhost\" }"
    }
}
```

## 2. Throwing Exceptions
You can intentionally throw exceptions using the `throw` keyword. You are not limited to throwing strings—you can throw dictionaries or custom objects containing rich error metadata.

```ez
task processPayment(amount) {
    when amount <= 0 {
        // Throwing a dictionary with metadata
        throw {
            "code": 400,
            "message": "Invalid payment amount",
            "critical": yes
        }
    }
    out "Payment of " + amount + " processed."
}

try {
    processPayment(-50)
} catch err {
    when typeOf(err) == "dictionary" {
        out "Error Code: " + err["code"]
        out "Details: " + err["message"]
    } other {
        out "Unknown error: " + err
    }
}
```

## 3. Finally
A `finally` block runs on **every** way out of a `try`: normal completion, a caught or uncaught exception (including one thrown from inside a `catch`), and a `give`, `escape` or `skip` that leaves the block. Use it to release things that must not leak.

What a `give` or `throw` inside `finally` does:

- A `finally` with no `give` leaves things alone: a pending return value or exception carries on once it finishes. The value of `give x` in the `try` is computed *before* the `finally` runs, so changing `x` there does not change what is returned.
- A `give` in `finally` **overrides** the pending return value and **discards** any exception that was in flight.
- A `throw` in `finally` **replaces** whatever was pending, a return value or an earlier exception.
- An exception thrown by a `finally` is not caught by the `catch` of the same `try`.

```ez
task load() {
    try {
        throw "disk unreadable"
    } catch err {
        throw "retry failed: " + err    // thrown from the catch...
    } finally {
        out "cleanup"                   // ...but finally still runs
        give "default"                  // and its give swallows the exception
    }
}
out load()          // cleanup, then: default

task counter() {
    n = 1
    try {
        give n
    } finally {
        n = 99                          // no give: the returned value is unchanged
    }
}
out counter()       // 1
```

## 4. The built-in `Exception` Object
EZ provides an `Exception` constructor that captures the call stack.
```ez
throw Exception("Database connection failed")
```

## 5. Edge Cases & Pitfalls
- **The `finally` Block**: EZ *does* support `finally` blocks, which execute after `try` and `catch` blocks. However, there is a known semantic edge case: if you `throw` a new exception *inside* the `catch` block, the `finally` block is bypassed and does not execute!
- **Uncaught Exceptions in Background Tasks**: Exceptions thrown inside a background thread (created via `spawn()`) will silently terminate that thread. The exception is **only** bubbled to the main thread when the main thread calls `await()` on that task's Future. If you never `await()` the Future, the error is lost entirely!
- **Catch Variable Shadowing**: The variable declared in the `catch` clause (e.g. `catch e`) creates a new lexical scope. It will shadow any existing variable named `e` in the outer scope for the duration of the catch block.
- **Throwing Nil**: `throw nil` is perfectly valid syntax, though generally discouraged as the `catch` block will just receive a `nil` payload with no context.
