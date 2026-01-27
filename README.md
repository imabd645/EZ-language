# EZ Programming Language

<div align="center">

![EZ Language](https://img.shields.io/badge/EZ-Language-blue?style=for-the-badge)
![Version](https://img.shields.io/badge/version-1.0-green?style=for-the-badge)
![License](https://img.shields.io/badge/license-MIT-orange?style=for-the-badge)

**A simple, readable programming language with natural syntax**

[Quick Start](#quick-start) • [Documentation](#documentation) • [Examples](#examples) • [Features](#features)

</div>

---

## 📋 Table of Contents

- [About EZ](#about-ez)
- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Language Syntax](#language-syntax)
  - [Variables](#variables)
  - [Data Types](#data-types)
  - [Operators](#operators)
  - [Control Flow](#control-flow)
  - [Functions](#functions)
  - [Arrays](#arrays)
  - [Dictionaries](#dictionaries)
  - [Object-Oriented Programming](#object-oriented-programming)
  - [Error Handling](#error-handling)
  - [Modules and Imports](#modules-and-imports)
- [Built-in Functions](#built-in-functions)
- [Package Manager](#package-manager)
- [Advanced Features](#advanced-features)
- [Examples](#examples)
- [Contributing](#contributing)
- [License](#license)

---

## 🎯 About EZ

**EZ** is a dynamically-typed, interpreted programming language designed with readability and simplicity in mind. It uses natural English-like keywords and intuitive syntax, making it perfect for beginners while being powerful enough for advanced use cases.

### Why EZ?

- ✅ **Natural Syntax**: Keywords like `out`, `when`, `repeat`, `task` instead of `print`, `if`, `for`, `function`
- ✅ **Beginner Friendly**: Easy to learn with clear, readable code
- ✅ **Feature Rich**: OOP, async/await, database support, HTTP requests
- ✅ **Garbage Collection**: Automatic memory management
- ✅ **Package Manager**: Built-in package management system
- ✅ **Interactive REPL**: Test code instantly
- ✅ **Cross-Platform**: Works on Windows, Linux, and macOS

---

## ✨ Features

### Core Features
- Dynamic typing with type inference
- First-class functions and closures
- Lambda expressions
- Array and dictionary literals
- String interpolation
- Pattern matching

### Advanced Features
- Object-oriented programming (classes and inheritance)
- Exception handling (try/catch)
- Async/await for concurrent operations
- SQLite database integration
- HTTP client (GET/POST requests)
- File system operations
- JSON parsing
- Regular expressions
- Threading support

### Developer Experience
- Interactive REPL mode
- Clear error messages with line numbers
- Package manager for code reuse
- Built-in documentation

---

## 🚀 Installation

### Prerequisites
- C++17 compatible compiler
- CMake (optional, for building)
- cURL library (for HTTP features)
- SQLite3 (for database features)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/imabd645/ez-language.git
cd ez-lang

# Compile (example using g++)
g++ -std=c++17 -o ez main.cpp Lexer.cpp Parser.cpp Interpreter.cpp Builtins.cpp \
    -lsqlite3 -lcurl -lpthread -lws2_32

# Run the interpreter
./ez
```

### Windows

```bash
g++ -std=c++17 -o ez.exe main.cpp Lexer.cpp Parser.cpp Interpreter.cpp Builtins.cpp \
    -lsqlite3 -lcurl -lws2_32 -lpthread
```

---

## 🎓 Quick Start

### Hello World

```ez
out "Hello, World!"
```

### Running a Script

Create a file `hello.ez`:
```ez
out "Hello, World!"
```

Run it:
```bash
./ez hello.ez
```

### Interactive Mode (REPL)

```bash
./ez
>>> out "Hello!"
Hello!
>>> x = 5 + 3
>>> out x
8
```

---

## 📚 Language Syntax

### Variables

Variables are dynamically typed and don't require declaration keywords.

```ez
// Simple assignment
x = 10
name = "Alice"
isActive = true

// Multiple assignments
a = b = c = 0

// Compound assignments
x += 5      // x = x + 5
y -= 2      // y = y - 2
z *= 3      // z = z * 3
w /= 4      // w = w / 4
```

### Data Types

EZ supports several built-in data types:

#### Numbers
```ez
integer = 42
decimal = 3.14
negative = -17
```

#### Strings
```ez
single = 'Hello'
double = "World"
escaped = "Line 1\nLine 2"
concatenation = "Hello" + " " + "World"
```

#### Booleans
```ez
isTrue = true
isFalse = false
```

#### Nil
```ez
empty = nil
```

#### Arrays
```ez
numbers = [1, 2, 3, 4, 5]
mixed = [1, "two", true, nil]
nested = [[1, 2], [3, 4]]
empty_array = []
```

#### Dictionaries
```ez
person = {
    "name": "Alice",
    "age": 30,
    "city": "New York"
}

// Access
out person["name"]  // Alice

// Set values
person["age"] = 31
```

### Operators

#### Arithmetic Operators
```ez
addition = 5 + 3        // 8
subtraction = 10 - 4    // 6
multiplication = 3 * 7  // 21
division = 20 / 4       // 5
modulo = 17 % 5         // 2
```

#### Comparison Operators
```ez
equal = (5 == 5)           // true
not_equal = (5 != 3)       // true
less = (3 < 5)             // true
less_equal = (5 <= 5)      // true
greater = (10 > 5)         // true
greater_equal = (7 >= 7)   // true
```

#### Logical Operators
```ez
and_op = true and false    // false
or_op = true or false      // true
not_op = not true          // false
```

#### Unary Operators
```ez
negation = -5              // -5
logical_not = not true     // false
```

### Control Flow

#### When Statement (If-Else)

```ez
// Simple when
when x > 5 {
    out "x is greater than 5"
}

// When-other (if-else)
when score >= 60 {
    out "Pass"
} other {
    out "Fail"
}

// Nested when statements
when age >= 18 {
    when hasLicense {
        out "Can drive"
    } other {
        out "Get a license first"
    }
} other {
    out "Too young to drive"
}

// Single line when
when x > 0 { out "Positive" }
```

#### While Loop

```ez
// Basic while loop
i = 0
while i < 5 {
    out i
    i += 1
}

// With escape (break)
count = 0
while true {
    when count >= 10 {
        escape  // breaks out of loop
    }
    count += 1
}

// With skip (continue)
n = 0
while n < 10 {
    n += 1
    when n % 2 == 0 {
        skip  // continues to next iteration
    }
    out n  // Only prints odd numbers
}
```

#### Repeat Loop (For Loop)

```ez
// Basic repeat loop
repeat i = 0 to 5 {
    out i  // Prints 0, 1, 2, 3, 4
}

// With custom step
repeat i = 0 to 10 {
    out i
    i += 2  // Custom increment
}

// Countdown
repeat i = 10 to 0 {
    out i
    i -= 1
}
```

#### Get Loop (For-Each)

```ez
// Iterate over array
fruits = ["apple", "banana", "orange"]
get fruit in fruits {
    out fruit
}

// Iterate over string characters
get char in "Hello" {
    out char
}

// With index
numbers = [10, 20, 30]
index = 0
get num in numbers {
    out "Index " + str(index) + ": " + str(num)
    index += 1
}
```

### Functions

#### Task Declaration (Function Definition)

```ez
// Basic function
task greet() {
    out "Hello!"
}

// Function with parameters
task greet_person(name) {
    out "Hello, " + name + "!"
}

// Function with return value
task add(a, b) {
    give a + b  // 'give' is return
}

// Function with multiple statements
task calculate(x) {
    result = x * 2
    result += 10
    give result
}

// Calling functions
greet()
greet_person("Alice")
sum = add(5, 3)
out sum  // 8
```

#### Lambda Expressions

```ez
// Simple lambda
square = (x) => x * x

// Lambda with multiple parameters
add = (a, b) => a + b

// Lambda with block body
compute = (x) => {
    temp = x * 2
    give temp + 5
}

// Using lambdas with arrays
numbers = [1, 2, 3, 4, 5]
get num in numbers {
    squared = square(num)
    out squared
}

// Higher-order functions
task apply(fn, value) {
    give fn(value)
}

result = apply((x) => x * 3, 10)  // 30
```

#### Closures

```ez
task makeCounter() {
    count = 0
    give () => {
        count += 1
        give count
    }
}

counter1 = makeCounter()
out counter1()  // 1
out counter1()  // 2
out counter1()  // 3

counter2 = makeCounter()
out counter2()  // 1 (separate closure)
```

### Arrays

#### Creating Arrays

```ez
// Array literal
numbers = [1, 2, 3, 4, 5]
empty = []

// Mixed types
mixed = [1, "two", true, nil, [1, 2]]
```

#### Array Operations

```ez
// Access elements
first = numbers[0]
last = numbers[4]

// Modify elements
numbers[0] = 10

// Length
size = len(numbers)

// Add elements
push(numbers, 6)        // [1, 2, 3, 4, 5, 6]

// Remove last element
last = pop(numbers)     // Returns and removes 6

// Iterate
get num in numbers {
    out num
}

// Nested arrays
matrix = [[1, 2], [3, 4]]
element = matrix[0][1]  // 2
```

### Dictionaries

```ez
// Create dictionary
person = {
    "name": "Alice",
    "age": 30,
    "email": "alice@example.com"
}

// Access values
name = person["name"]

// Set values
person["age"] = 31
person["phone"] = "555-1234"

// Get keys
keys = person.keys()

// Get values
values = person.values()

// Check if key exists
hasEmail = person.contains("email")

// Length
size = len(person)

// Iterate over keys
get key in keys(person) {
    out key + ": " + str(person[key])
}
```

### Object-Oriented Programming

#### Defining a Model (Class)

```ez
model Person {
    // Constructor
    init(name, age) {
        self.name = name
        self.age = age
    }
    
    // Public method (shown)
    shown greet() {
        out "Hello, I'm " + self.name
    }
    
    // Private method (hidden)
    hidden calculateBirthYear() {
        give 2024 - self.age
    }
    
    // Public method using private method
    shown getBirthYear() {
        give self.calculateBirthYear()
    }
}

// Create instance
alice = new Person("Alice", 30)
alice.greet()  // "Hello, I'm Alice"
out alice.name  // "Alice"
year = alice.getBirthYear()
```

#### Inheritance

```ez
model Student extends Person {
    init(name, age, studentId) {
        self.name = name
        self.age = age
        self.studentId = studentId
    }
    
    shown study() {
        out self.name + " is studying"
    }
}

bob = new Student("Bob", 20, "S12345")
bob.greet()   // Inherited from Person
bob.study()   // Student's own method
```

#### Struct (Simplified Class)

```ez
struct Point {
    x, y
}

// Structs are simpler than models
// They automatically create fields and basic constructor
p = new Point()
p.x = 10
p.y = 20
```

### Error Handling

```ez
// Try-catch block
try {
    result = 10 / 0
    out result
} catch error {
    out "Error occurred: " + str(error)
}

// Throwing errors
task divide(a, b) {
    when b == 0 {
        throw "Division by zero"
    }
    give a / b
}

try {
    result = divide(10, 0)
} catch e {
    out "Caught error: " + str(e)
}

// Nested try-catch
try {
    try {
        throw "Inner error"
    } catch e {
        out "Inner catch"
        throw "Outer error"
    }
} catch e {
    out "Outer catch: " + str(e)
}
```

### Modules and Imports

```ez
// Import a module
use "mymodule.ez"

// Use functions from imported module
// The module's code is executed and variables are available
```

---

## 🔧 Built-in Functions

### Input/Output

| Function | Description | Example |
|----------|-------------|---------|
| `out(value)` | Print to console | `out "Hello"` |
| `in()` | Read input from user | `name = in()` |

### Type Conversion

| Function | Description | Example |
|----------|-------------|---------|
| `str(x)` | Convert to string | `str(42)` → `"42"` |
| `num(x)` | Convert to number | `num("42")` → `42` |
| `type(x)` | Get type name | `type(42)` → `"number"` |

### String Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `len(s)` | string | String length | `len("hello")` → `5` |
| `substr(s, start, len)` | string, number, number | Extract substring | `substr("hello", 1, 3)` → `"ell"` |
| `split(s, delim)` | string, string | Split string into array | `split("a,b,c", ",")` → `["a","b","c"]` |
| `join(arr, delim)` | array, string | Join array into string | `join(["a","b"], ",")` → `"a,b"` |
| `replace(s, old, new)` | string, string, string | Replace substring | `replace("hello", "l", "L")` → `"heLLo"` |
| `trim(s)` | string | Remove whitespace | `trim(" hi ")` → `"hi"` |
| `upper(s)` | string | Convert to uppercase | `upper("hi")` → `"HI"` |
| `lower(s)` | string | Convert to lowercase | `lower("HI")` → `"hi"` |
| `indexOf(s, sub)` | string, string | Find substring index | `indexOf("hello", "ll")` → `2` |
| `charAt(s, index)` | string, number | Get character at index | `charAt("hello", 1)` → `"e"` |
| `startsWith(s, prefix)` | string, string | Check if starts with | `startsWith("hello", "he")` → `true` |
| `endsWith(s, suffix)` | string, string | Check if ends with | `endsWith("hello", "lo")` → `true` |

### Array Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `len(arr)` | array | Array length | `len([1,2,3])` → `3` |
| `push(arr, val)` | array, any | Add element | `push(arr, 5)` |
| `pop(arr)` | array | Remove & return last | `pop([1,2,3])` → `3` |
| `slice(arr, start, end)` | array, number, number | Extract slice | `slice([1,2,3,4], 1, 3)` → `[2,3]` |
| `reverse(arr)` | array | Reverse array | `reverse([1,2,3])` → `[3,2,1]` |
| `sort(arr)` | array | Sort array | `sort([3,1,2])` → `[1,2,3]` |
| `contains(arr, val)` | array, any | Check if contains | `contains([1,2,3], 2)` → `true` |
| `indexOf(arr, val)` | array, any | Find element index | `indexOf([1,2,3], 2)` → `1` |
| `range(start, end)` | number, number | Create range array | `range(1, 5)` → `[1,2,3,4,5]` |
| `map(arr, fn)` | array, function | Apply function to each | `map([1,2,3], (x)=>x*2)` → `[2,4,6]` |
| `filter(arr, fn)` | array, function | Filter by condition | `filter([1,2,3,4], (x)=>x>2)` → `[3,4]` |
| `reduce(arr, fn, init)` | array, function, any | Reduce to single value | `reduce([1,2,3], (a,b)=>a+b, 0)` → `6` |

### Math Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `abs(x)` | number | Absolute value | `abs(-5)` → `5` |
| `floor(x)` | number | Round down | `floor(3.7)` → `3` |
| `ceil(x)` | number | Round up | `ceil(3.2)` → `4` |
| `round(x)` | number | Round to nearest | `round(3.5)` → `4` |
| `sqrt(x)` | number | Square root | `sqrt(16)` → `4` |
| `pow(x, y)` | number, number | Power | `pow(2, 3)` → `8` |
| `min(a, b)` | number, number | Minimum | `min(5, 3)` → `3` |
| `max(a, b)` | number, number | Maximum | `max(5, 3)` → `5` |
| `rand()` | none | Random 0-1 | `rand()` → `0.543` |
| `randint(min, max)` | number, number | Random integer | `randint(1, 10)` → `7` |
| `sin(x)` | number | Sine | `sin(0)` → `0` |
| `cos(x)` | number | Cosine | `cos(0)` → `1` |
| `tan(x)` | number | Tangent | `tan(0)` → `0` |

### File System Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `read_file(path)` | string | Read file content | `content = read_file("data.txt")` |
| `write_file(path, data)` | string, string | Write to file | `write_file("out.txt", "Hello")` |
| `append_file(path, data)` | string, string | Append to file | `append_file("log.txt", "Entry")` |
| `file_exists(path)` | string | Check if file exists | `file_exists("test.txt")` → `true` |
| `delete_file(path)` | string | Delete file | `delete_file("temp.txt")` |
| `list_dir(path)` | string | List directory | `files = list_dir(".")` |
| `create_dir(path)` | string | Create directory | `create_dir("newfolder")` |
| `file_size(path)` | string | Get file size | `size = file_size("data.txt")` |

### JSON Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `json_parse(str)` | string | Parse JSON string | `obj = json_parse('{"a":1}')` |
| `json_stringify(obj)` | any | Convert to JSON | `json_stringify({"a": 1})` → `'{"a":1}'` |

### Database Functions (SQLite)

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `db_open(path)` | string | Open database | `db = db_open("data.db")` |
| `db_execute(db, sql)` | database, string | Execute SQL | `db_execute(db, "CREATE TABLE...")` |
| `db_query(db, sql)` | database, string | Query database | `rows = db_query(db, "SELECT * FROM...")` |
| `db_close(db)` | database | Close database | `db_close(db)` |
| `db_last_insert_id(db)` | database | Get last insert ID | `id = db_last_insert_id(db)` |
| `db_begin(db)` | database | Begin transaction | `db_begin(db)` |
| `db_commit(db)` | database | Commit transaction | `db_commit(db)` |
| `db_rollback(db)` | database | Rollback transaction | `db_rollback(db)` |

### HTTP Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `http_get(url)` | string | GET request | `response = http_get("https://api.example.com")` |
| `http_post(url, body)` | string, string | POST request | `http_post(url, '{"key":"value"}')` |
| `fetch(url, options)` | string, dict | Async HTTP request | `future = fetch(url, {"method": "GET"})` |

### Async Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `spawn(fn, args...)` | function, any... | Run function async | `future = spawn(myFunc, arg1, arg2)` |
| `await(future)` | future | Wait for result | `result = await(future)` |
| `sync(future)` | future | Alias for await | `result = sync(future)` |
| `sleep(ms)` | number | Sleep milliseconds | `sleep(1000)` |

### Regular Expression Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `regex_match(str, pattern)` | string, string | Check if matches | `regex_match("hello", "h.*o")` → `true` |
| `regex_replace(str, pattern, repl)` | string, string, string | Replace matches | `regex_replace("hello", "l+", "L")` |
| `regex_search(str, pattern)` | string, string | Find first match | `regex_search("test123", "\\d+")` → `"123"` |

### Dictionary Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `keys(dict)` | dictionary | Get all keys | `keys({"a": 1, "b": 2})` → `["a", "b"]` |
| `values(dict)` | dictionary | Get all values | `values({"a": 1, "b": 2})` → `[1, 2]` |
| `has_key(dict, key)` | dictionary, string | Check if key exists | `has_key(dict, "name")` → `true` |

### Utility Functions

| Function | Parameters | Description | Example |
|----------|------------|-------------|---------|
| `clock()` | none | Milliseconds since epoch | `clock()` → `1699123456789` |
| `print(values...)` | any... | Print multiple values | `print("x=", x, "y=", y)` |

---

## 📦 Package Manager

EZ comes with a built-in package manager for sharing and reusing code.

### Commands

```bash
# Install a package
ez install package-name [version]

# List installed packages
ez list

# Initialize a new package
ez init my-package
```

### Creating a Package

1. Create a `package.ez` file:
```json
{
  "name": "my-package",
  "version": "1.0.0",
  "description": "My awesome package",
  "author": "Your Name",
  "main": "main.ez",
  "dependencies": {
    "other-package": "1.0.0"
  }
}
```

2. Create your `main.ez` file with your package code

3. Upload to GitHub as `EZmy-package`

### Using a Package

```ez
// In your code
use "my-package"

// Use functions from the package
result = myPackageFunction()
```

---

## 🚀 Advanced Features

### Async/Await Programming

```ez
// Spawn async task
task fetchData(url) {
    response = http_get(url)
    give response
}

// Run asynchronously
future1 = spawn(fetchData, "https://api1.com")
future2 = spawn(fetchData, "https://api2.com")

// Wait for results
data1 = await(future1)
data2 = await(future2)

out "Got both responses!"
```

### Async HTTP with Fetch

```ez
// Make async HTTP request
future = fetch("https://api.example.com/data", {
    "method": "POST",
    "body": json_stringify({"key": "value"}),
    "headers": {
        "Content-Type": "application/json"
    }
})

// Do other work while waiting
out "Request sent, doing other work..."

// Wait for response
response = await(future)
data = json_parse(response)
out data
```

### Database Operations

```ez
// Open database
db = db_open("myapp.db")

// Create table
db_execute(db, "
    CREATE TABLE IF NOT EXISTS users (
        id INTEGER PRIMARY KEY,
        name TEXT NOT NULL,
        email TEXT UNIQUE
    )
")

// Insert data
db_execute(db, "
    INSERT INTO users (name, email) 
    VALUES ('Alice', 'alice@example.com')
")

// Get last insert ID
userId = db_last_insert_id(db)

// Query data
results = db_query(db, "SELECT * FROM users")
get row in results {
    out "User: " + row["name"] + " - " + row["email"]
}

// Transaction
db_begin(db)
try {
    db_execute(db, "INSERT INTO users (name, email) VALUES ('Bob', 'bob@example.com')")
    db_execute(db, "INSERT INTO users (name, email) VALUES ('Carol', 'carol@example.com')")
    db_commit(db)
} catch error {
    db_rollback(db)
    out "Transaction failed: " + str(error)
}

// Close database
db_close(db)
```

### Regular Expressions

```ez
// Match pattern
text = "Contact: user@example.com"
isEmail = regex_match(text, ".*@.*\\.com")
out isEmail  // true

// Extract data
phoneText = "Call me at 555-1234"
phone = regex_search(phoneText, "\\d{3}-\\d{4}")
out phone  // "555-1234"

// Replace
censored = regex_replace("password123", "\\d+", "***")
out censored  // "password***"
```

### File Operations

```ez
// Read file
content = read_file("data.txt")
out content

// Write file
write_file("output.txt", "Hello, World!")

// Append to file
append_file("log.txt", "New log entry\n")

// Check if exists
when file_exists("config.txt") {
    config = read_file("config.txt")
} other {
    out "Config file not found!"
}

// List directory
files = list_dir(".")
get file in files {
    out file
}

// Create directory
create_dir("output")

// Get file size
size = file_size("data.txt")
out "File size: " + str(size) + " bytes"
```

### Higher-Order Functions

```ez
// Map
numbers = [1, 2, 3, 4, 5]
doubled = map(numbers, (x) => x * 2)
out doubled  // [2, 4, 6, 8, 10]

// Filter
evens = filter(numbers, (x) => x % 2 == 0)
out evens  // [2, 4]

// Reduce
sum = reduce(numbers, (acc, x) => acc + x, 0)
out sum  // 15

// Chaining
result = reduce(
    filter(
        map(numbers, (x) => x * 2),
        (x) => x > 5
    ),
    (acc, x) => acc + x,
    0
)
out result  // 20 (6 + 8 + 10)
```

---

## 💡 Examples

### Example 1: Calculator

```ez
task calculator() {
    out "Simple Calculator"
    out "Enter first number:"
    a = num(in())
    
    out "Enter operator (+, -, *, /):"
    op = in()
    
    out "Enter second number:"
    b = num(in())
    
    when op == "+" {
        give a + b
    } other when op == "-" {
        give a - b
    } other when op == "*" {
        give a * b
    } other when op == "/" {
        when b == 0 {
            throw "Division by zero!"
        }
        give a / b
    } other {
        throw "Invalid operator"
    }
}

try {
    result = calculator()
    out "Result: " + str(result)
} catch error {
    out "Error: " + str(error)
}
```

### Example 2: Todo List

```ez
todos = []

task addTodo(task) {
    todo = {
        "id": len(todos) + 1,
        "task": task,
        "done": false
    }
    push(todos, todo)
}

task listTodos() {
    when len(todos) == 0 {
        out "No todos yet!"
        give
    }
    
    get todo in todos {
        status = todo["done"] ? "[✓]" : "[ ]"
        out status + " " + str(todo["id"]) + ". " + todo["task"]
    }
}

task markDone(id) {
    get todo in todos {
        when todo["id"] == id {
            todo["done"] = true
            give
        }
    }
    throw "Todo not found"
}

// Usage
addTodo("Buy groceries")
addTodo("Learn EZ")
addTodo("Build something cool")

listTodos()
markDone(2)
out "\nAfter marking #2 done:"
listTodos()
```

### Example 3: Web Scraper

```ez
task fetchQuote() {
    // Fetch from quotes API
    response = http_get("https://api.quotable.io/random")
    quote = json_parse(response)
    
    give quote
}

// Fetch multiple quotes asynchronously
futures = []
repeat i = 0 to 5 {
    future = spawn(fetchQuote)
    push(futures, future)
}

// Wait and display
out "Fetching 5 random quotes...\n"
get future in futures {
    quote = await(future)
    out "\"" + quote["content"] + "\""
    out "   - " + quote["author"] + "\n"
}
```

### Example 4: Class Inheritance

```ez
model Animal {
    init(name) {
        self.name = name
    }
    
    shown speak() {
        out self.name + " makes a sound"
    }
}

model Dog extends Animal {
    init(name, breed) {
        self.name = name
        self.breed = breed
    }
    
    shown speak() {
        out self.name + " barks!"
    }
    
    shown getBreed() {
        give self.breed
    }
}

model Cat extends Animal {
    init(name, color) {
        self.name = name
        self.color = color
    }
    
    shown speak() {
        out self.name + " meows!"
    }
}

// Create instances
dog = new Dog("Rex", "German Shepherd")
cat = new Cat("Whiskers", "Orange")

dog.speak()  // "Rex barks!"
cat.speak()  // "Whiskers meows!"
out "Breed: " + dog.getBreed()
```

### Example 5: Database CRUD

```ez
task initDatabase() {
    db = db_open("contacts.db")
    db_execute(db, "
        CREATE TABLE IF NOT EXISTS contacts (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            name TEXT NOT NULL,
            phone TEXT,
            email TEXT
        )
    ")
    give db
}

task addContact(db, name, phone, email) {
    db_execute(db, "
        INSERT INTO contacts (name, phone, email)
        VALUES ('" + name + "', '" + phone + "', '" + email + "')
    ")
    give db_last_insert_id(db)
}

task getAllContacts(db) {
    give db_query(db, "SELECT * FROM contacts")
}

task deleteContact(db, id) {
    db_execute(db, "DELETE FROM contacts WHERE id = " + str(id))
}

// Usage
db = initDatabase()

// Add contacts
addContact(db, "Alice", "555-1234", "alice@example.com")
addContact(db, "Bob", "555-5678", "bob@example.com")

// List contacts
contacts = getAllContacts(db)
out "All Contacts:"
get contact in contacts {
    out str(contact["id"]) + ". " + contact["name"] + " - " + contact["phone"]
}

db_close(db)
```

---

## 🤝 Contributing

We welcome contributions! Here's how you can help:

### Reporting Bugs
- Use the GitHub issue tracker
- Include code samples that reproduce the issue
- Specify your OS and compiler version

### Suggesting Features
- Open an issue with the "feature request" label
- Explain the use case and benefits
- Provide examples if possible

### Submitting Pull Requests
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Make your changes
4. Add tests if applicable
5. Commit with clear messages (`git commit -m 'Add amazing feature'`)
6. Push to your fork (`git push origin feature/amazing-feature`)
7. Open a Pull Request

### Code Style Guidelines
- Use 4 spaces for indentation
- Follow existing naming conventions
- Comment complex logic
- Keep functions focused and small
- Write clear commit messages

---

## 📖 Documentation

For more detailed documentation:
- [Language Specification](docs/specification.md)
- [API Reference](docs/api.md)
- [Tutorial](docs/tutorial.md)
- [FAQ](docs/faq.md)

---

## 🛣️ Roadmap

### Version 1.1 (Upcoming)
- [ ] Improved error messages
- [ ] Debugger support
- [ ] Standard library expansion
- [ ] Performance optimizations

### Version 1.2 (Future)
- [ ] Native GUI library
- [ ] Package registry website
- [ ] VS Code extension
- [ ] Documentation generator

### Version 2.0 (Long-term)
- [ ] JIT compilation
- [ ] Static typing (optional)
- [ ] Native mobile support
- [ ] WebAssembly target

---

## ❓ FAQ

### Is EZ suitable for production?
EZ is currently in active development. While it's stable for small to medium projects, we recommend thorough testing before production use.

### How does EZ compare to Python/JavaScript?
EZ aims for similar simplicity to Python but with more natural English keywords. It's dynamically typed like both but has its own unique syntax focused on readability.

### Can I contribute built-in functions?
Yes! We welcome contributions. Check the contributing guidelines above.

### Does EZ support Unicode?
Currently, EZ supports ASCII strings. Unicode support is planned for a future release.

### How fast is EZ?
As an interpreted language, EZ prioritizes development speed over runtime speed. It's suitable for scripts, automation, and small applications. JIT compilation is planned for future versions.

---

## 📄 License

EZ is released under the MIT License. See [LICENSE](LICENSE) for details.

```
MIT License

Copyright (c) 2024 EZ Language Team

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## 🙏 Acknowledgments

- Inspired by Python, Lua, and Lox
- Built with modern C++17
- Uses SQLite, cURL, and other open-source libraries
- Thanks to all contributors!

---

## 📞 Contact

- **GitHub**: [github.com/imabd645/ez-language](https://github.com/imabd645/ez-language)
- **Issues**: [github.com/imabd645/ez-language/issues](https://github.com/imabd645/ez-language/issues)
- **Discussions**: [github.com/imabd645/ez-language/discussions](https://github.com/imabd645/ez-language/discussions)

---



**Made By Abdullah Masood**

⭐ Star us on GitHub if you find EZ useful!

[Getting Started](#quick-start) • [Documentation](#documentation) • [Examples](#examples) • [Contributing](#contributing)

</div>