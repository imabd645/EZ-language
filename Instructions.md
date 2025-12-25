# EZ Programming Language Documentation

**EZ Interpreter v2.1** - A simple, readable programming language with natural syntax.

## Table of Contents
- [Getting Started](#getting-started)
- [Basic Syntax](#basic-syntax)
- [Variables](#variables)
- [Data Types](#data-types)
- [Operators](#operators)
- [Control Flow](#control-flow)
- [Loops](#loops)
- [Functions](#functions)
- [Arrays](#arrays)
- [Input/Output](#inputoutput)
- [Comments](#comments)
- [Complete Examples](#complete-examples)

---

## Getting Started

### Installation
download the ez.exe file and add in the path through environmnet variables

### Running a Program
```bash
ez hello.ez
```

---

## Basic Syntax

EZ uses simple, English-like keywords to make programming intuitive.

### Hello World
```ez
out "Hello, World!"
```

---

## Variables

Variables are created automatically when you assign a value to them.

```ez
x = 10
name = "Alice"
isActive = yes
```

### Variable Rules
- No declaration needed
- Variable names can contain letters, numbers, and underscores
- Case-sensitive
- Cannot start with a number

---

## Data Types

### 1. Numbers
```ez
age = 25
pi = 3.14159
negative = -42
```

### 2. Strings
```ez
greeting = "Hello"
message = "Welcome to EZ!"
```

**Escape Sequences:**
- `\n` - Newline
- `\t` - Tab
- `\\` - Backslash
- `\"` - Quote

```ez
text = "Line 1\nLine 2"
```

### 3. Booleans
```ez
isTrue = yes
isFalse = no
```

### 4. Arrays
```ez
numbers = [1, 2, 3, 4, 5]
names = ["Alice", "Bob", "Charlie"]
mixed = [1, "text", yes, 3.14]
```

---

## Operators

### Arithmetic Operators
| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition / Concatenation | `5 + 3` → `8` |
| `-` | Subtraction | `5 - 3` → `2` |
| `*` | Multiplication | `5 * 3` → `15` |
| `/` | Division | `10 / 2` → `5` |
| `%` | Modulo | `10 % 3` → `1` |

```ez
sum = 5 + 3
product = 4 * 2
```

### String Concatenation
```ez
firstName = "John"
lastName = "Doe"
fullName = firstName + " " + lastName
out fullName  // John Doe
```

### Comparison Operators
| Operator | Word Form | Description | Example |
|----------|-----------|-------------|---------|
| `==` | `equal` | Equal to | `x equal 5` |
| `!=` | | Not equal to | `x != 5` |
| `<` | `less` | Less than | `x less 10` |
| `>` | `greater` | Greater than | `x greater 5` |
| `<=` | `lessthen` | Less than or equal | `x lessthen 10` |
| `>=` | `greaterthen` | Greater than or equal | `x greaterthen 5` |

```ez
when x equal 10 {
    out "x is 10"
}

when age greater 18 {
    out "Adult"
}
```

### Logical Operators
| Operator | Description | Example |
|----------|-------------|---------|
| `and` | Logical AND | `x > 0 and x < 10` |
| `or` | Logical OR | `x equal 5 or x equal 10` |
| `not` | Logical NOT | `not isActive` |

```ez
when age greater 18 and hasLicense equal yes {
    out "Can drive"
}
```

**Note:** EZ uses short-circuit evaluation - the right side of `and`/`or` is only evaluated if necessary.

### Increment/Decrement
```ez
x = 5
x++     // x becomes 6
x--     // x becomes 5
```

---

## Control Flow

### If-Else Statement

**Keyword:** `when` (if), `other` (else)

```ez
when condition {
    // code if true
}

when condition {
    // code if true
} other {
    // code if false
}
```

**Example:**
```ez
age = 20

when age greater 18 {
    out "You are an adult"
} other {
    out "You are a minor"
}
```

**Nested Conditions:**
```ez
score = 85

when score greaterthen 90 {
    out "Grade: A"
} other {
    when score greaterthen 80 {
        out "Grade: B"
    } other {
        out "Grade: C"
    }
}
```

---

## Loops

### For Loop (Range-Based)

**Keyword:** `repeat...to`

```ez
repeat variable = start to end {
    // code
}
```

**Example:**
```ez
repeat i = 1 to 5 {
    out i
}
// Output: 1 2 3 4 5
```

**Countdown:**
```ez
repeat i = 5 to 1 {
    out i
}
```

### While Loop

**Keyword:** `until`

```ez
until condition {
    // code
}
```

**Example:**
```ez
x = 0
until x less 5 {
    out x
    x++
}
// Output: 0 1 2 3 4
```

**Infinite Loop (use with caution):**
```ez
until yes {
    out "Running..."
    // Remember to use 'escape' to break!
}
```

### Loop Control

**Keywords:** `escape` (break), `skip` (continue)

```ez
repeat i = 1 to 10 {
    when i equal 5 {
        skip    // Skip iteration when i is 5
    }
    when i equal 8 {
        escape  // Exit loop when i is 8
    }
    out i
}
// Output: 1 2 3 4 6 7
```

---

## Functions

### Function Definition

**Keyword:** `task` (function), `give` (return)

```ez
task functionName(param1, param2) {
    // code
    give result
}
```

**Example:**
```ez
task add(a, b) {
    give a + b
}

result = add(5, 3)
out result  // 8
```

### Functions Without Return
```ez
task greet(name) {
    out "Hello, " + name + "!"
}

greet("Alice")  // Hello, Alice!
```

### Recursive Functions
```ez
task factorial(n) {
    when n lessthen 2 {
        give 1
    }
    give n * factorial(n - 1)
}

out factorial(5)  // 120
```

**Note:** Maximum recursion depth is 1000 to prevent stack overflow.

---

## Arrays

### Creating Arrays
```ez
numbers = [1, 2, 3, 4, 5]
empty = []
```

### Accessing Elements
```ez
numbers = [10, 20, 30, 40]
out numbers[0]  // 10
out numbers[2]  // 30
```

**Note:** Arrays are 0-indexed (first element is at index 0).

### Modifying Elements
```ez
numbers = [1, 2, 3]
numbers[1] = 99
out numbers[1]  // 99
```

### Arrays in Loops
```ez
colors = ["red", "green", "blue"]

repeat i = 0 to 2 {
    out colors[i]
}
```

---

## Input/Output

### Output

**Keyword:** `out`

```ez
out "Hello"
out 42
out 3.14

x = 100
out x

out "Result: " + x
```

### Input

**Keyword:** `in`

```ez
in variableName
```

**Example:**
```ez
out "Enter your name: "
in name
out "Hello, " + name + "!"

out "Enter your age: "
in age
out "You are " + age + " years old"
```

**Note:** Input is automatically parsed as a number if possible, otherwise stored as a string.

---

## Comments

Use `//` for single-line comments:

```ez
// This is a comment
x = 10  // Variable assignment

// This is ignored
// Multiple comment lines
```

---

## Complete Examples

### Example 1: Calculator
```ez
task calculator(a, b, op) {
    when op equal "+" {
        give a + b
    }
    when op equal "-" {
        give a - b
    }
    when op equal "*" {
        give a * b
    }
    when op equal "/" {
        give a / b
    }
    give 0
}

out calculator(10, 5, "+")   // 15
out calculator(10, 5, "*")   // 50
```

### Example 2: Fizz Buzz
```ez
repeat i = 1 to 20 {
    when i % 15 equal 0 {
        out "FizzBuzz"
    } other {
        when i % 3 equal 0 {
            out "Fizz"
        } other {
            when i % 5 equal 0 {
                out "Buzz"
            } other {
                out i
            }
        }
    }
}
```

### Example 3: Fibonacci Sequence
```ez
task fibonacci(n) {
    when n lessthen 2 {
        give n
    }
    give fibonacci(n - 1) + fibonacci(n - 2)
}

repeat i = 0 to 10 {
    out fibonacci(i)
}
```

### Example 4: Array Sum
```ez
task sumArray(arr, size) {
    total = 0
    repeat i = 0 to size - 1 {
        total = total + arr[i]
    }
    give total
}

numbers = [10, 20, 30, 40, 50]
result = sumArray(numbers, 5)
out "Sum: " + result  // Sum: 150
```

### Example 5: Guessing Game
```ez
secret = 42
guesses = 0

out "Guess the number (1-100):"

until no {
    in guess
    guesses++
    
    when guess equal secret {
        out "Correct! You won in " + guesses + " guesses!"
        escape
    }
    
    when guess less secret {
        out "Too low! Try again:"
    } other {
        out "Too high! Try again:"
    }
}
```

### Example 6: Temperature Converter
```ez
task celsiusToFahrenheit(c) {
    give (c * 9 / 5) + 32
}

task fahrenheitToCelsius(f) {
    give (f - 32) * 5 / 9
}

out "Enter temperature in Celsius:"
in temp
out temp + "°C = " + celsiusToFahrenheit(temp) + "°F"
```

---

## Keywords Reference

| Keyword | Purpose | Example |
|---------|---------|---------|
| `out` | Print output | `out "Hello"` |
| `in` | Get input | `in name` |
| `when` | If statement | `when x > 5 { }` |
| `other` | Else statement | `other { }` |
| `repeat` | For loop | `repeat i = 1 to 10 { }` |
| `to` | Range end in loop | `repeat i = 1 to 10` |
| `until` | While loop | `until x < 10 { }` |
| `task` | Define function | `task add(a, b) { }` |
| `give` | Return value | `give result` |
| `escape` | Break from loop | `escape` |
| `skip` | Continue loop | `skip` |
| `yes` | Boolean true | `isActive = yes` |
| `no` | Boolean false | `isActive = no` |
| `and` | Logical AND | `x > 0 and x < 10` |
| `or` | Logical OR | `x equal 5 or x equal 10` |
| `not` | Logical NOT | `not isActive` |
| `equal` | Equality comparison | `x equal 5` |
| `greater` | Greater than | `x greater 5` |
| `greaterthen` | Greater than or equal | `x greaterthen 5` |
| `less` | Less than | `x less 10` |
| `lessthen` | Less than or equal | `x lessthen 10` |

---

## Error Handling

EZ provides detailed error messages with line numbers:

```
Error on line 5: Division by zero
Error on line 12: Undefined variable: count
Error on line 8: Array index out of bounds: 10
```

### Common Errors

1. **Division by Zero**
   ```ez
   x = 10 / 0  // Error!
   ```

2. **Undefined Variable**
   ```ez
   out myVar  // Error if myVar not defined
   ```

3. **Array Out of Bounds**
   ```ez
   arr = [1, 2, 3]
   out arr[5]  // Error!
   ```

4. **Type Mismatch**
   ```ez
   x = "text" - 5  // Error!
   ```

5. **Maximum Recursion**
   ```ez
   task infinite() {
       give infinite()  // Error after 1000 calls
   }
   ```

---

## Tips and Best Practices

1. **Use meaningful variable names**
   ```ez
   // Good
   userName = "Alice"
   totalScore = 100
   
   // Bad
   x = "Alice"
   y = 100
   ```

2. **Comment your code**
   ```ez
   // Calculate area of circle
   radius = 5
   area = 3.14159 * radius * radius
   ```

3. **Keep functions small and focused**
   ```ez
   // Do one thing well
   task square(n) {
       give n * n
   }
   ```

4. **Use arrays for collections**
   ```ez
   // Instead of score1, score2, score3...
   scores = [95, 87, 92, 88, 91]
   ```

5. **Check conditions before operations**
   ```ez
   when divisor != 0 {
       result = dividend / divisor
   }
   ```

---

## Limitations

- Maximum recursion depth: 1000 levels
- No file I/O operations
- No object-oriented features
- No string slicing/manipulation functions
- Arrays cannot be resized dynamically

---

## Contributing

Found a bug or want to add a feature? Contributions are welcome!

1. Fork the repository
2. Create your feature branch
3. Commit your changes
4. Push to the branch
5. Create a Pull Request

---

## License

This project is open source and available under the MIT License.

---

## Support

For questions, issues, or suggestions, please open an issue on GitHub.

Happy coding with EZ! 🚀
