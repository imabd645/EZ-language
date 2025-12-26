# EZ Language - Complete Documentation

A comprehensive guide to learning and using EZ Language.

---

## Table of Contents

1. [Introduction](#introduction)
2. [Installation](#installation)
3. [Getting Started](#getting-started)
4. [Language Basics](#language-basics)
5. [Variables & Data Types](#variables--data-types)
6. [Operators](#operators)
7. [Control Flow](#control-flow)
8. [Loops](#loops)
9. [Functions](#functions)
10. [Arrays](#arrays)
11. [Built-in Functions](#built-in-functions)
12. [VS Code Extension](#vs-code-extension)
13. [Examples](#examples)
14. [Troubleshooting](#troubleshooting)

---

## Introduction

EZ Language is a beginner-friendly programming language designed to make learning to code as easy as possible. It uses intuitive, human-readable keywords that feel natural to write and read.

### Why EZ?

- **Natural Syntax**: Keywords like `when`, `repeat`, and `task` read like English
- **No Semicolons**: Focus on your logic, not punctuation
- **Readable Booleans**: Use `yes` and `no` instead of cryptic `true/false`
- **Quick Learning Curve**: Start coding in minutes, not hours

---

## Installation

### Step 1: Download EZ Interpreter

Download `ez.exe` from the official website or GitHub releases.

### Step 2: Create Installation Folder

Create a folder for EZ Language:
```
C:\EZ
```

Move `ez.exe` to this folder.

### Step 3: Add to System PATH

Adding EZ to your PATH allows you to run it from any location.

1. Press `Win + R` to open Run dialog
2. Type `sysdm.cpl` and press Enter
3. Click the **Advanced** tab
4. Click **Environment Variables**
5. Under **System variables**, find **Path** and click **Edit**
6. Click **New**
7. Type: `C:\EZ`
8. Click **OK** on all dialogs

### Step 4: Verify Installation

Open a new Command Prompt and type:
```bash
ez --version
```

If installed correctly, you'll see the version number.

---

## Getting Started

### Your First Program

Create a file named `hello.ez`:

```
out "Hello, World!"
```

Run it:
```bash
ez hello.ez
```

Output:
```
Hello, World!
```

### Program Structure

EZ programs are executed from top to bottom. No main function is required.

```
// This is a comment
out "This runs first"
out "This runs second"
```

---

## Language Basics

### Comments

```
// This is a single-line comment
```

### Output

Use `out` to print to the console:

```
out "Hello!"
out 42
out "The answer is: " + 42
```

### Input

Use `in` to get user input:

```
out "What is your name?"
in name
out "Hello, " + name + "!"
```

---

## Variables & Data Types

### Declaring Variables

Variables are created by assignment:

```
name = "Abdullah"
age = 25
pi = 3.14159
isStudent = yes
```

### Data Types

| Type | Example | Description |
|------|---------|-------------|
| String | `"Hello"` | Text in quotes |
| Number | `42`, `3.14` | Integers and decimals |
| Boolean | `yes`, `no` | True/false values |
| Array | `[1, 2, 3]` | Collection of values |

### String Operations

```
firstName = "Abdullah"
lastName = "Masood"

// Concatenation
fullName = firstName + " " + lastName
out fullName  // Abdullah Masood
```

---

## Operators

### Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition | `5 + 3` → `8` |
| `-` | Subtraction | `5 - 3` → `2` |
| `*` | Multiplication | `5 * 3` → `15` |
| `/` | Division | `6 / 3` → `2` |
| `%` | Modulo | `5 % 3` → `2` |

### Comparison Operators

| EZ Operator | Symbol | Description |
|-------------|--------|-------------|
| `equal` | `==` | Equal to |
| `notequal` | `!=` | Not equal to |
| `greater` | `>` | Greater than |
| `less` | `<` | Less than |
| `greaterthen` | `>=` | Greater than or equal |
| `lessthen` | `<=` | Less than or equal |

### Logical Operators

| EZ Operator | Symbol | Description |
|-------------|--------|-------------|
| `and` | `&&` | Both must be true |
| `or` | `\|\|` | At least one true |
| `not` | `!` | Negation |

---

## Control Flow

### If Statement (`when`)

```
age = 20

when age greaterthen 18 {
    out "You are an adult"
}
```

### If-Else (`when` / `other`)

```
age = 15

when age greaterthen 18 {
    out "Adult"
} other {
    out "Minor"
}
```

### If-Else If-Else

```
score = 75

when score greaterthen 90 {
    out "Grade: A"
} other when score greaterthen 80 {
    out "Grade: B"
} other when score greaterthen 70 {
    out "Grade: C"
} other {
    out "Grade: F"
}
```

---

## Loops

### For Loop (`repeat`)

```
// Count from 1 to 5
repeat i = 1 to 5 {
    out i
}
```

Output:
```
1
2
3
4
5
```

### While Loop (`until`)

```
x = 5
until x equal 0 {
    out x
    x = x - 1
}
```

Output:
```
5
4
3
2
1
```

### Foreach Loop (`get`)

```
fruits = ["apple", "banana", "orange"]
get fruit in fruits {
    out fruit
}
```

### Loop Control

**Break out of loop:**
```
repeat i = 1 to 10 {
    when i equal 5 {
        escape
    }
    out i
}
```

**Skip to next iteration:**
```
repeat i = 1 to 5 {
    when i equal 3 {
        skip
    }
    out i
}
```

---

## Functions

### Defining Functions (`task`)

```
task greet(name) {
    out "Hello, " + name + "!"
}

// Call the function
greet("World")
```

### Return Values (`give`)

```
task add(a, b) {
    give a + b
}

result = add(5, 3)
out result  // 8
```

### Multiple Parameters

```
task introduce(name, age, city) {
    out name + " is " + age + " years old from " + city
}

introduce("Abdullah", 25, "Lahore")
```

### Recursive Functions

```
task factorial(n) {
    when n lessthen 2 {
        give 1
    }
    give n * factorial(n - 1)
}

out factorial(5)  // 120
```

---

## Arrays

### Creating Arrays

```
numbers = [1, 2, 3, 4, 5]
names = ["Alice", "Bob", "Charlie"]
mixed = [1, "hello", yes, 3.14]
```

### Accessing Elements

```
fruits = ["apple", "banana", "orange"]
out fruits[0]  // apple
out fruits[1]  // banana
```

### Array Operations

```
colors = ["red", "green"]

// Add element
add(colors, "blue")

// Remove element
remove(colors, 0)

// Get length
out len(colors)
```

### Iterating Arrays

```
numbers = [10, 20, 30, 40, 50]

get num in numbers {
    out num * 2
}
```

---

## Built-in Functions

| Function | Description | Example |
|----------|-------------|---------|
| `len(x)` | Get length | `len("hello")` → `5` |
| `add(arr, val)` | Add to array | `add(list, "item")` |
| `remove(arr, idx)` | Remove from array | `remove(list, 0)` |
| `clock()` | Get current time | `time = clock()` |
| `stop(ms)` | Pause execution | `stop(1000)` |

---

## VS Code Extension

### Installation

1. Download `ez-language.vsix`
2. Open VS Code
3. Press `Ctrl + Shift + X`
4. Click `...` → **Install from VSIX...**
5. Select the file

### Available Snippets

Type these prefixes and press `Tab`:

| Prefix | Expands To |
|--------|------------|
| `when` | If statement |
| `whenother` | If-else statement |
| `repeat` | For loop |
| `until` | While loop |
| `task` | Function definition |
| `get` | Foreach loop |
| `out` | Print statement |
| `in` | Input statement |

---

## Examples

### Calculator

```
out "Simple Calculator"
out "Enter first number:"
in num1
out "Enter second number:"
in num2
out "Enter operation (+, -, *, /):"
in op

when op equal "+" {
    out num1 + num2
} other when op equal "-" {
    out num1 - num2
} other when op equal "*" {
    out num1 * num2
} other when op equal "/" {
    out num1 / num2
}
```

### Fibonacci Sequence

```
task fib(n) {
    when n lessthen 2 {
        give n
    }
    give fib(n - 1) + fib(n - 2)
}

out "Fibonacci Sequence:"
repeat i = 0 to 10 {
    out fib(i)
}
```

### Prime Checker

```
task isPrime(n) {
    when n lessthen 2 {
        give no
    }
    repeat i = 2 to n - 1 {
        when n % i equal 0 {
            give no
        }
    }
    give yes
}

repeat num = 1 to 20 {
    when isPrime(num) {
        out num + " is prime"
    }
}
```

### Guess the Number

```
secret = 7

out "Guess the number (1-10):"
in guess

until guess equal secret {
    when guess less secret {
        out "Too low! Try again:"
    } other {
        out "Too high! Try again:"
    }
    in guess
}

out "Congratulations! You got it!"
```

---

## Troubleshooting

### Common Errors

**"ez is not recognized"**
- Make sure you added the EZ folder to your PATH
- Open a new Command Prompt after adding to PATH

**"File not found"**
- Check that the file extension is `.ez`
- Make sure you're in the correct directory

**Syntax errors**
- Check for matching braces `{ }`
- Ensure strings are in double quotes `"`
- Verify keyword spelling

### Getting Help

- Visit the [GitHub repository](https://github.com/imabd645/EZ-language)
- Check the [examples folder](examples/)
- Open an issue for bugs

---

<p align="center">
  <b>Happy Coding with EZ! 🚀</b>
</p>
