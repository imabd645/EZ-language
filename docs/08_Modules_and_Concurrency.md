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

### `await` and `sync`
Once a task is spawned, you wait for its result with `await`:

```ez
finalResult = await(futureWork)
out "Math result: " + str(finalResult)
```

`sync` is an **alias for `await`** — both names are registered to the same
function in `Builtins_GC.cpp`, so they behave identically. There is no
"blocking" variant to choose between.

### Waiting on many futures

| Builtin | Behaviour |
|---|---|
| `awaitAll(futures)` | Waits for all; results in input order. Throws on the first failure |
| `awaitAny(futures)` | Waits for the **first to settle** — if that one failed, this fails |
| `isDone(future)` | Has it finished? Non-blocking |
| `cancel(future)` | Cancels; awaiting it afterwards throws |

```ez
results = awaitAll([f1, f2, f3])       // ["...", "...", "..."]

when not isDone(f1) { out "still running" }
```

For richer patterns — first *success* rather than first *settled*, per-future
outcomes that never throw, timeouts, retries, bounded worker pools — use the
`thread` package (`allSettled`, `any`, `withTimeout`, `retry`, `WorkerPool`).

### Errors cross the future

If a spawned task throws, the failure is recorded on its future. Awaiting
re-raises it, so it is catchable where you wait:

```ez
try {
    await(spawn(mightFail))
} catch (e) {
    out "task failed: " + str(e.message)
}
```

An error that crossed a future boundary arrives as an exception **instance** —
read `e.message`. `str()` on an instance gives `<instance>`. A local
`throw "text"` is caught as a plain string.

### `Channel` — passing values between threads

```ez
ch = Channel()
spawn(| | { ch.send("done") })
ch.receive()                  // blocks until a value arrives
ch.receiveTimeout(500)        // or nil after 500ms
ch.tryReceive()               // nil if nothing queued; never blocks
```

Backed by a real mutex and condition variable, so receivers sleep in the OS
rather than polling.

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
- **Global Variable Race Conditions**: Spawned threads share the same VM environment and global memory, so several threads mutating the *same* global array or dictionary at once will race. Keep background work to local scope, or guard shared state with the built-in `mutex()` / `lock(m, fn)` (a real `std::mutex` released even if the body throws), `Atomic(n)` for counters, or a `Channel` to hand values across instead of sharing them.
- **Circular Imports**: If File A `use`s File B, and File B `use`s File A, the VM will enter an infinite import loop until it crashes via Stack Overflow. Carefully architect your dependency trees to avoid circular references.
- **Multiple `await` Calls**: Calling `await` on the *same* `EZFuture` multiple times is perfectly safe. It will return the cached result immediately on the second call.
