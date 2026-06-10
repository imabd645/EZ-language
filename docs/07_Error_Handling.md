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
    when typeOf(err) == "Dictionary" {
        out "Error Code: " + err["code"]
        out "Details: " + err["message"]
    } other {
        out "Unknown error: " + err
    }
}
```

## 3. The built-in `Exception` Object
EZ provides an `Exception` constructor that captures the call stack.
```ez
throw Exception("Database connection failed")
```

## 4. Edge Cases & Pitfalls
- **No `finally` Block**: EZ does not currently have a `finally` keyword. If you open a file or a network socket, you must remember to close it at the end of your `try` block AND inside your `catch` block to avoid leaks.
- **Uncaught Exceptions in Background Tasks**: Exceptions thrown inside a background thread (created via `spawn()`) will silently terminate that thread. The exception is **only** bubbled to the main thread when the main thread calls `await()` on that task's Future. If you never `await()` the Future, the error is lost entirely!
- **Catch Variable Shadowing**: The variable declared in the `catch` clause (e.g. `catch e`) creates a new lexical scope. It will shadow any existing variable named `e` in the outer scope for the duration of the catch block.
- **Throwing Nil**: `throw nil` is perfectly valid syntax, though generally discouraged as the `catch` block will just receive a `nil` payload with no context.
