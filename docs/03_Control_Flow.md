# Control Flow & Iteration

EZ's control flow syntax replaces traditional `if/else` and `for` loops with more readable keywords.

## 1. Conditional Logic (`when` / `other`)
```ez
age = 25
hasLicense = yes

when age < 16 {
    out "Cannot drive."
} when age >= 16 and hasLicense == false {
    out "Needs to pass the test."
} other {
    out "Can drive legally."
}
```

## 2. Pattern Matching (`match`)
The `match` statement is a powerful replacement for switch statements. It compares a subject against multiple arms.
```ez
statusCode = 404

match statusCode {
    200 => {
        out "Success!"
        out "Data loaded."
    }
    404 => out "Error: Not Found"
    500 => out "Error: Server Failure"
    other => out "Unknown status code: " + statusCode
}
```

## 3. Iteration

### `while` Loop
```ez
task factorial(n) {
    result = 1
    while n > 1 {
        result = result * n
        n = n - 1
    }
    give result
}
out factorial(5) // 120
```

### `repeat` Loop (Ranges)
The upper bound is exclusive.
```ez
// Builds an array from 0 to 4
arr = []
repeat i = 0 to 5 {
    arr[] = i
}
```

### `get` Loop (Iterators)
The safest and most efficient way to iterate collections.

**Iterating Arrays:**
```ez
systems = ["Linux", "Windows", "macOS"]
get os in systems {
    out "Checking compatibility for: " + os
}
```

**Iterating Dictionaries with Destructuring:**
You can extract both the key and the value simultaneously using `[key, value]`.
```ez
config = {
    "host": "127.0.0.1",
    "port": 3000,
    "timeout": 5000
}

get [key, val] in config {
    out "Config -> " + key + ": " + val
}
```

## 4. Loop Control Keywords
- `escape`: Breaks completely out of the loop block.
- `skip`: Stops the current iteration and jumps to the next condition evaluation.

```ez
get num in [1, 2, 3, 4, 5, 6] {
    when num == 3 {
        out "Skipping 3"
        skip
    }
    when num == 5 {
        out "Found 5, escaping loop"
        escape
    }
    out "Processing: " + num
}
```

## 5. Edge Cases & Pitfalls
- **Truthy / Falsy Nuances**: In EZ, conditions are strictly evaluated. Only explicit `false` and `nil` evaluate to a boolean false in conditions. `0`, `""` (empty string), and `[]` (empty array) all evaluate to **true**.
  ```ez
  when 0 {
      out "This WILL print because 0 is truthy!"
  }
  ```
- **Match Fallthrough**: The `match` statement does *not* fall through. Once an arm is matched and executed, the block automatically jumps to the end of the match statement. There is no need for a `break` keyword.
- **Match without 'other'**: If a `match` statement fails to match any pattern and does not include an `other` clause, it silently completes without error.
- **Modifying Iterables**: Never add or remove elements from an Array or Dictionary while inside a `get` loop iterating over it! This can cause iterator invalidation and crash the VM.
- **Loop Variable Scope**: The loop variable in `get` and `repeat` statements is lexically scoped to the loop block. Attempting to access it outside the loop will result in an "undefined variable" error.
