# ⚡ Concurrency, Tasks & Multithreading

EZ provides first-class support for both **Asynchronous Non-Blocking I/O** (via libuv) and **True Parallel Multithreading** (via worker VMs).

---

## 1. 🧵 Spawning Worker Threads (`spawn`)

`spawn(function, ...args)` creates an isolated worker thread running a parallel VM instance:

```ez
task heavyComputation(n) {
    total = 0
    repeat i = 1 to n {
        total = total + i
    }
    give total
}

// Spawn background worker thread
future = spawn(heavyComputation, 1000000)

out "Worker running in background..."

// Wait for result
result = await(future)
out "Result: " + str(result)
```

### Daemon Threads
By default, the program will stay alive until all spawned threads finish executing. If you want a background thread to automatically exit when the main thread exits, pass `{ "daemon": true }` or `true` as the final argument to `spawn()`:
```ez
spawn(backgroundHeartbeatTask, { "daemon": true })
// or
spawn(backgroundHeartbeatTask, true)
```

---

## 2. ⏳ Asynchronous Futures (`await`)

Futures represent values that will become available in the future (from `spawn()`, async HTTP requests, or timers):

```ez
// Non-blocking timer
timer = waitAsync(1000) // 1000ms
out "Waiting for timer..."
await(timer)
out "Timer expired!"
```

---

## 3. 🔒 Thread Synchronization: Channels & Mutexes

### Channels (Thread-Safe Queues)
```ez
chan = Channel(10) // Capacity = 10

task producer(ch) {
    repeat i = 1 to 5 {
        ch.send(i)
    }
    ch.close()
}

spawn(producer, chan)

// Consume from channel
while true {
    val = chan.receive()
    when val == nil {
        break
    }
    out "Received: " + str(val)
}
```

### Mutexes (`mutex` / `lock`)
```ez
m = mutex()
counter = 0

lock(m, lambda() {
    counter = counter + 1
})
```

---

## 4. 🛡️ Native Thread-Safe Data Structures

One of EZ's most powerful concurrency features is **fine-grained object-level locking**. 

You do **not** need a `mutex` to safely share `Arrays` or `Dictionaries` across multiple threads! All core composite operations (`push()`, `pop()`, reading/writing indexes) are automatically protected by highly optimized read-write locks (`std::shared_mutex` under the hood) per object.

```ez
sharedData = {}
sharedArray = []

// Spawn 100 threads that all safely mutate the SAME array simultaneously
repeat i = 1 to 100 {
    spawn(lambda() {
        sharedData[str(i)] = i
        push(sharedArray, i)
    })
}
// This will never segfault or corrupt data in EZ!
```
