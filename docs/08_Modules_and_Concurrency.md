# Modules and Concurrency

## Modules (`use`)
You can split your EZ code into multiple files and import them using `use`.

```ez
// In math_utils.ez
task add(a, b) { give a + b }

// In main.ez
use "math_utils"
out add(5, 10)
```

## Concurrency
EZ uses event-based native threading for asynchronous execution.

- **spawn**: Runs a task asynchronously and returns an `EZFuture` object.
- **await**: Waits for a future to complete and returns its result without blocking the main event loop.
- **sync**: A blocking wait for a future to complete.

```ez
task heavyComputation() {
    // some heavy work
    give 42
}

future = spawn(heavyComputation)
out "Waiting..."
result = await(future)
out "Result: " + result
```
