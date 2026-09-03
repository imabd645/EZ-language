# `runtime` — Standard Runtime & Interpreter Controls for EZ

The `runtime` package provides programmatic control and introspection over the EZ Bytecode Virtual Machine, Garbage Collector, execution profiler, call stack, system processes, and platform engine metadata.

---

## Quick Start

```ez
use "runtime" as rt

# Check engine info
out "Running EZ " + rt.info.version + " on " + rt.info.platform

# Dynamic recursion depth control
rt.vm.setMaxRecursionDepth(10000)

# Garbage collection control
rt.gc.collect()
```

---

## Table of Contents
1. [Garbage Collector (`rt.gc`)](#1-garbage-collector-rtgc)
2. [Virtual Machine & Stack Limits (`rt.vm`)](#2-virtual-machine--stack-limits-rtvm)
3. [Performance & Instruction Profiler (`rt.perf`)](#3-performance--instruction-profiler-rtperf)
4. [Call Stack Introspection (`rt.stack`)](#4-call-stack-introspection-rtstack)
5. [System & Process (`rt.sys`)](#5-system--process-rtsys)
6. [Engine & Platform Info (`rt.info`)](#6-engine--platform-info-rtinfo)
7. [Code Recipes & Examples](#7-code-recipes--examples)

---

## 1. Garbage Collector (`rt.gc`)

The `rt.gc` controller provides fine-grained control over the Cycle Collector and heap tracking.

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `enable()` | `bool` | Resumes automatic cycle collection. |
| `disable()` | `bool` | Temporarily suspends automatic cycle collection. |
| `isEnabled()` | `bool` | Returns `true` if the collector is currently active, `false` otherwise. |
| `collect()` | `bool` | Releases stale stack slots, cleans unreferenced OS file handles, and forces a full cycle collection sweep. |
| `tracked()` | `number` | Returns the total count of container objects currently monitored by the cycle collector. |
| `cyclesCollected()` | `number` | Returns the cumulative count of cyclic references reclaimed since process launch. |
| `setThresholds(minIntervalMs, maxObjects)` | `bool` | Adjusts the minimum time interval and object-count triggers for automated collections. |
| `stats()` | `dictionary` | Returns a summary dictionary: `{"tracked": N, "cycles": N, "enabled": bool}`. |

### Example:
```ez
use "runtime" as rt

# Suspend GC during critical, high-throughput loops
rt.gc.disable()

# Perform memory-heavy operations...

# Resume and force a sweep
rt.gc.enable()
rt.gc.collect()

stats = rt.gc.stats()
out "Tracked objects: " + str(stats["tracked"])
```

---

## 2. Virtual Machine & Stack Limits (`rt.vm`)

The `rt.vm` controller allows inspecting and dynamically modifying VM execution bounds, such as call-stack frame depth.

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `getMaxRecursionDepth()` | `number` | Returns the maximum permitted call stack depth (default: `4096`). |
| `setMaxRecursionDepth(depth)` | `bool` | Sets a new maximum call stack depth. Prevents runaway recursion or allows deep recursion algorithms. |
| `currentDepth()` | `number` | Returns the number of active call frames currently on the stack. |

### Example:
```ez
use "runtime" as rt

out "Default depth: " + str(rt.vm.getMaxRecursionDepth()) # 4096

# Expand limit for deep recursive tree traversal
rt.vm.setMaxRecursionDepth(20000)

task deepRecurse(n) {
    when n <= 0 { give 0 }
    give deepRecurse(n - 1) + 1
}

result = deepRecurse(5000)
out "Result: " + str(result)
```

---

## 3. Performance & Instruction Profiler (`rt.perf`)

The `rt.perf` controller measures bytecode instruction throughput and execution duration.

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `instructions()` | `number` | Returns the total count of bytecode instructions dispatched by the VM so far. |
| `resetInstructions()` | `bool` | Resets the instruction counter to zero. |
| `now()` | `number` | Returns high-precision system time in milliseconds. |
| `time(fn)` | `number` | Executes `fn` and returns elapsed milliseconds. |

### Example:
```ez
use "runtime" as rt

rt.perf.resetInstructions()

duration = rt.perf.time(|| {
    sum = 0
    repeat i = 1 to 10000 {
        sum = sum + i
    }
})

out "Elapsed: " + str(duration) + " ms"
out "Bytecode instructions executed: " + str(rt.perf.instructions())
```

---

## 4. Call Stack Introspection (`rt.stack`)

The `rt.stack` controller allows capturing live call stack backtraces and inspecting executing frames.

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `trace()` | `array` | Returns an array of dictionaries representing active call frames, innermost-first: `[{"function": string, "file": string, "line": number}, ...]`. |
| `currentDepth()` | `number` | Returns the current call stack depth. |
| `maxDepth()` | `number` | Alias for `rt.vm.getMaxRecursionDepth()`. |
| `setMaxDepth(depth)` | `bool` | Alias for `rt.vm.setMaxRecursionDepth(depth)`. |

### Example:
```ez
use "runtime" as rt

task logStackTrace() {
    frames = rt.stack.trace()
    out "--- Stack Trace (" + str(len(frames)) + " frames) ---"
    get frame in frames {
        out "  at " + frame["function"] + " (" + frame["file"] + ":" + str(frame["line"]) + ")"
    }
}

task executePipeline() {
    logStackTrace()
}

executePipeline()
```

---

## 5. System & Process (`rt.sys`)

The `rt.sys` controller interfaces with OS process handles, working directories, and command-line arguments.

| Method | Return Type | Description |
| :--- | :--- | :--- |
| `argv()` | `array` | Returns the command-line arguments passed to the script as an array of strings. |
| `pid()` | `number` | Returns the operating system Process ID (PID). |
| `cwd()` | `string` | Returns the absolute path of the current working directory. |
| `exit(code)` | `void` | Immediately terminates the process with the specified exit code (default: `0`). |

### Example:
```ez
use "runtime" as rt

args = rt.sys.argv()
out "PID: " + str(rt.sys.pid())
out "Working Directory: " + rt.sys.cwd()
out "Arguments: " + str(args)

when len(args) == 0 {
    out "No arguments provided, exiting..."
    rt.sys.exit(1)
}
```

---

## 6. Engine & Platform Info (`rt.info`)

The `rt.info` controller provides read-only properties describing the build, target architecture, and engine characteristics.

| Property | Type | Description / Example |
| :--- | :--- | :--- |
| `version` | `string` | Language and runtime version (e.g. `"5.0.0"`). |
| `engine` | `string` | `"EZ Bytecode VM"`. |
| `platform` | `string` | `"windows"`, `"linux"`, or `"macos"`. |
| `arch` | `string` | `"x86_64"`, `"arm64"`, or `"x86"`. |
| `pointerSize` | `number` | Memory pointer width in bytes (`8` for 64-bit systems). |
| `endianness` | `string` | `"little"` or `"big"`. |
| `compiler` | `string` | C++ host compiler used to build the runtime (e.g. `"GCC 15.2.0"`, `"Clang 17.0"`). |
| `getAll()` | `dictionary` | Returns all engine properties in a single consolidated dictionary. |

### Example:
```ez
use "runtime" as rt

out "Engine:       " + rt.info.engine
out "Version:      " + rt.info.version
out "Platform:     " + rt.info.platform + " (" + rt.info.arch + ")"
out "Host Compiler:" + rt.info.compiler
```

---

## 7. Code Recipes & Examples

### Recipe 1: Benchmarking Block
```ez
use "runtime" as rt

task benchmark(name, fn) {
    rt.perf.resetInstructions()
    ms = rt.perf.time(fn)
    ins = rt.perf.instructions()
    out "[" + name + "] Time: " + str(ms) + "ms | Instructions: " + str(ins)
}

benchmark("Array Allocation", || {
    arr = []
    repeat i = 1 to 5000 { push(arr, i) }
})
```

### Recipe 2: Safe Recursion Guard
```ez
use "runtime" as rt

# Temporarily restrict stack depth to prevent runaway processing
previousMax = rt.vm.getMaxRecursionDepth()
rt.vm.setMaxRecursionDepth(100)

try {
    # Run user-supplied recursive plugin/handler
    runUntrustedLogic()
} catch e {
    out "Execution halted: " + str(e)
} finally {
    # Always restore original depth limit
    rt.vm.setMaxRecursionDepth(previousMax)
}
```
