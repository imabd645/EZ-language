# Frequently Asked Questions

## Syntax

### Why `when` instead of `if`?
EZ prioritizes readability and distinctiveness. `when` implies a condition to be met, and `other` handles the alternative cases naturally.

### Why `task` instead of `function`?
In EZ, code units are seen as "tasks" to be performed. This aligns with the language's native support for asynchronous `task` spawning.

## Features

### Is EZ object-oriented?
Yes, EZ supports class-based OOP with `model`, inheritance (`extends`), and encapsulation (`hidden`/`shown`).

### Can I build web apps?
Yes! EZ has a built-in `server` function and HTTP client (`fetch`), making it suitable for backend development.

### How do I handle errors?
Use `try` and `catch`. You can `throw` any value as an error.

```javascript
try {
    fail_code()
} catch e {
    out "Error: " + e
}
```

## Performance

### Is it compiled?
EZ is an interpreted language written in C++. It parses source code into an AST and executes it directly.
