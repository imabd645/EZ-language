# Error Handling

EZ handles runtime errors through exceptions.

## Try / Catch
Use `try` to wrap risky code and `catch` to handle exceptions gracefully.

```ez
try {
    result = 10 / 0
} catch e {
    out "An error occurred: " + e
}
```

## Throwing Exceptions
You can manually throw exceptions using the `throw` keyword.

```ez
task divide(a, b) {
    when b == 0 {
        throw "Cannot divide by zero!"
    }
    give a / b
}
```
