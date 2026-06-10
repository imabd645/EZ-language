# Modules and Concurrency

## 1. Modules (`use`)
The `use` keyword allows you to modularize your application by evaluating an external EZ script within the current execution environment.

**File: `math.ez`**
```ez
task square(x) {
    give x * x
}
```

**File: `main.ez`**
```ez
use "math.ez"

out square(10) // 100
```
*Note: `use` expects a relative file path.*

## 2. Multi-threading Concurrency
EZ leverages real native OS threads for concurrency. It does not use fake cooperative coroutines. Background tasks run completely in parallel to the main thread.

### `spawn`
Starts a task on a background OS thread. It returns an `EZFuture` object instantly.
```ez
task heavyMathWork(seed) {
    result = seed
    repeat i = 0 to 1000000 {
        result = result + sqrt(i)
    }
    give result
}

// Spawns immediately
futureWork = spawn(heavyMathWork, 50)
out "Work is running in the background..."
```

### `await` vs `sync`
Once a task is spawned, you must wait for its result. EZ provides two distinct keywords for this:
- **`await`**: Yields the current task's execution back to the VM event loop until the future completes. This is absolutely critical in GUI applications to ensure the window doesn't freeze!
- **`sync`**: Blocks the underlying OS thread completely. Use this only in CLI tools where event loop responsiveness doesn't matter.

```ez
// Non-blocking wait
finalResult = await(futureWork)
out "Math result: " + finalResult
```

### Concurrent Fetch Example
```ez
task downloadPage(url) {
    out "Fetching " + url + "..."
    // Simulating delay
    stop(1000)
    give "HTML content of " + url
}

f1 = spawn(downloadPage, "google.com")
f2 = spawn(downloadPage, "bing.com")
f3 = spawn(downloadPage, "yahoo.com")

// The main thread waits, but all 3 tasks run in parallel!
res1 = await(f1)
res2 = await(f2)
res3 = await(f3)

out "All downloads finished."
```

## 3. Edge Cases & Pitfalls
- **Global Variable Race Conditions**: Since spawned threads share the exact same VM environment and global memory state, multiple threads modifying the *same* global array or dictionary simultaneously will cause a race condition, potentially crashing the VM garbage collector. Always limit background tasks to local scope manipulations or ensure thread safety via mutexes (if implemented by your native bindings).
- **Deadlocking via `sync`**: Calling `sync` on the main thread for a future that requires the main thread's event loop to resolve will cause an unbreakable deadlock. Always use `await` unless you have a specific reason not to.
- **Circular Imports**: If File A `use`s File B, and File B `use`s File A, the VM will enter an infinite import loop until it crashes via Stack Overflow. Carefully architect your dependency trees to avoid circular references.
- **Multiple `await` Calls**: Calling `await` on the *same* `EZFuture` multiple times is perfectly safe. It will return the cached result immediately on the second call.
