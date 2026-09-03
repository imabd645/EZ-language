# `sandbox` — Secure Execution Sandbox for EZ

The `sandbox` package provides a secure, isolated execution environment for evaluating untrusted user scripts, plugins, formulas, and templates in the EZ programming language with capability-based security, stack limits, and CPU instruction quotas.

---

## Quick Start

```ez
use "sandbox" as sb

# 1. Create a sandbox with strict isolation
box = sb.create({
    "allowFS": false,          # Disallow all file operations
    "allowNet": false,         # Disallow network requests
    "allowProcess": false,     # Disallow process termination (exit) & spawning
    "maxRecursionDepth": 50,   # Prevent stack overflow attacks
    "maxInstructions": 50000   # CPU budget to interrupt infinite loops
})

# 2. Inject custom variables and safe callbacks
box.inject("PI", 3.14159)
box.inject("discount", |price| { give price * 0.9 })

# 3. Safely evaluate user-supplied code
result = box.eval("
    raw = 100
    give discount(raw) * PI
")
out "Result: " + str(result)
```

---

## Table of Contents
1. [Security Capabilities & Permissions](#1-security-capabilities--permissions)
2. [CPU Budget & Infinite Loop Protection](#2-cpu-budget--infinite-loop-protection)
3. [Stack Overflow Protection](#3-stack-overflow-protection)
4. [Custom Globals & Callback Injection](#4-custom-globals--callback-injection)
5. [API Reference](#5-api-reference)

---

## 1. Security Capabilities & Permissions

Configure access to sensitive native subsystems:

| Option | Default | Description |
| :--- | :---: | :--- |
| `allowFS` | `false` | Controls access to `readFile`, `writeFile`, `appendFile`, `removeFile`, `readLines`, `File`. |
| `allowNet` | `false` | Controls access to `http_get`, `http_post`, `fetch`. |
| `allowProcess` | `false` | Controls access to `exit`, `spawn`, `system`, `panic`, `__process_pid`, `__process_cwd`. |
| `allowFFI` | `false` | Controls access to native DLL loading and raw memory operations. |
| `maxRecursionDepth` | `50` | Maximum call stack depth before throwing `RecursionError`. |
| `maxInstructions` | `100000` | Maximum bytecode instruction count before throwing `SecurityError`. |

---

## 2. CPU Budget & Infinite Loop Protection

If an untrusted script enters a runaway or infinite loop (such as `while true {}`), the sandbox's instruction quota interrupts execution without freezing the host process:

```ez
use "sandbox" as sb

box = sb.create({ "maxInstructions": 1000 })

try {
    box.eval("
        while true {
            # Runaway infinite loop
        }
    ")
} catch e {
    out "Caught attack: " + str(e)
    # Output: Caught attack: SecurityError: Instruction limit exceeded (CPU budget exhausted)
}
```

---

## 3. Stack Overflow Protection

Protects against recursive bombs:

```ez
box = sb.create({ "maxRecursionDepth": 25 })

try {
    box.eval("
        task bomb(n) { give bomb(n + 1) }
        bomb(1)
    ")
} catch e {
    out "Caught recursion: " + str(e)
    # Output: Caught recursion: RecursionError: max recursion depth (25) exceeded
}
```

---

## 4. Custom Globals & Callback Injection

Inject any number of safe constants, math utilities, dictionaries, arrays, or callback functions:

```ez
use "sandbox" as sb

box = sb.create()

# Inject individual variables
box.inject("USER_TIER", "premium")
box.inject("log", |msg| { out "[SANDBOX]: " + str(msg) })

# Inject batch dictionary
box.injectAll({
    "TAX_RATES": { "CA": 0.0725, "NY": 0.08875 },
    "currency": "USD"
})

output = box.eval("
    log('Calculating NY tax...')
    give 100 * TAX_RATES['NY']
")
```
