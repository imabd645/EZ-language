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
