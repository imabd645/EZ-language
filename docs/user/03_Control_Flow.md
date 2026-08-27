# 🔀 Control Flow & Loops

This guide covers conditional branching, loops, and pattern matching in EZ.

---

## 1. 🔀 Conditional Branching: `when` / `other`

EZ uses `when` and `other` for conditional decision making:

```ez
score = 85

when score >= 90 {
    out "Grade: A"
} other when score >= 80 {
    out "Grade: B"
} other when score >= 70 {
    out "Grade: C"
} other {
    out "Grade: F"
}
```

---

## 2. 🔄 Loops

### `while` Loop
Repeats a block as long as the condition evaluates to truthy:
```ez
count = 0
while count < 5 {
    out "Count: " + str(count)
    count = count + 1
}
```

### `repeat` Loop (Numeric Ranges)
Fast, optimized integer iteration:
```ez
// Loop from 1 to 10 inclusive
repeat i = 1 to 10 {
    out "Iteration: " + str(i)
}

// Loop with custom step
repeat i = 0 to 100 by 10 {
    out "Step: " + str(i)
}
```

### `for ... in` Iteration
Iterate over arrays, strings, and dictionaries:
```ez
items = ["apple", "banana", "cherry"]

for fruit in items {
    out "Fruit: " + fruit
}
```

---

## 3. 🛑 Loop Control: `break` & `continue`

- **`break`**: Immediately terminates the innermost loop.
- **`continue`**: Jumps directly to the next iteration of the loop.

```ez
repeat i = 1 to 10 {
    when i == 3 {
        continue    // Skip 3
    }
    when i == 8 {
        break       // Stop at 8
    }
    out i
}
```
