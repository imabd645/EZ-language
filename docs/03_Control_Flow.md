# Control Flow

EZ offers a variety of control flow structures for conditional logic and iteration.

## Conditional Logic (`when` / `other`)
Use `when` instead of `if`, and `other` instead of `else`.
```ez
when age >= 18 {
    out "Adult"
} other {
    out "Minor"
}
```

## Pattern Matching (`match`)
The `match` statement evaluates a subject against multiple patterns.
```ez
match status {
    200 => out "OK"
    404 => out "Not Found"
    other => out "Unknown Status"
}
```

## Loops

### While Loop
```ez
count = 0
while count < 5 {
    out count
    count = count + 1
}
```

### Repeat Loop
Iterates a variable from a start value up to (but not including) an end value.
```ez
repeat i = 0 to 5 {
    out i
}
```

### Get Loop (Iterators)
Used to iterate over arrays and dictionaries.
```ez
// Array Iteration
fruits = ["Apple", "Banana", "Cherry"]
get fruit in fruits {
    out fruit
}

// Dictionary Iteration
scores = {"Alice": 90, "Bob": 85}
get [name, score] in scores {
    out name + ": " + score
}
```

### Loop Control
- `escape`: Breaks out of the current loop (like `break`).
- `skip`: Skips to the next iteration of the loop (like `continue`).
