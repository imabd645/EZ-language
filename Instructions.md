# EZ Language - Complete Instructions

EZ is a simple, beginner-friendly interpreted programming language with an intuitive English-like syntax. This guide covers everything you need to know to write programs in EZ.

## Table of Contents

1. [Getting Started](#getting-started)
2. [Basic Syntax](#basic-syntax)
3. [Data Types](#data-types)
4. [Variables](#variables)
5. [Operators](#operators)
6. [Control Flow](#control-flow)
7. [Functions](#functions)
8. [Arrays](#arrays)
9. [Built-in Functions](#built-in-functions)
10. [File Operations](#file-operations)
11. [Web Server](#web-server)
12. [Libraries](#libraries)
13. [Example Programs](#example-programs)

---

## Getting Started

### Compilation

Compile the EZ interpreter:

```bash
g++ -std=c++17 -o ez ez.cpp -lws2_32
```

### Running Programs

```bash
ez program.ez
```

### Hello World

```ez
out "Hello, World!"
```

---

## Basic Syntax

### Comments

```ez
/// This is a comment (three slashes)
out "This will print"  /// Comments can be at end of line
```

### Output

```ez
out "Hello"           /// Print text
out 42                /// Print numbers
out x, y, z           /// Print multiple values (space-separated)
```

### Input

```ez
in username           /// Read input into variable
out "Hello, " + username
```

---

## Data Types

### Numbers

```ez
x = 42                /// Integer
y = 3.14              /// Float
z = -10               /// Negative
```

### Strings

```ez
name = "Alice"
message = "Hello, World!"
escaped = "Line 1\nLine 2\tTabbed"
```

### Booleans

```ez
isTrue = yes          /// true
isFalse = no          /// false
```

### Arrays

```ez
numbers = [1, 2, 3, 4, 5]
mixed = [1, "hello", yes, 3.14]
empty = []
```

---

## Variables

### Assignment

```ez
x = 10
name = "Bob"
```

### Compound Assignment

```ez
x += 5                /// Add and assign
x -= 3                /// Subtract and assign
x *= 2                /// Multiply and assign
x /= 4                /// Divide and assign
x %= 3                /// Modulo and assign
```

### Increment/Decrement

```ez
counter++             /// Increment by 1
counter--             /// Decrement by 1
```

---

## Operators

### Arithmetic

```ez
a + b                 /// Addition
a - b                 /// Subtraction
a * b                 /// Multiplication
a / b                 /// Division (float)
a // b                /// Integer division
a % b                 /// Modulo
a ** b                /// Power (exponentiation)
```

### Comparison

You can use either symbols or English keywords:

```ez
/// Symbol syntax
a equal b  or  a == b        /// Equal
a less b  or  a < b           /// Less than
a greater b  or  a > b        /// Greater than
a lessthan b  or  a <= b      /// Less than or equal (also: lessthen)
a greaterthan b  or  a >= b   /// Greater than or equal (also: greaterthen)
a notequal b  or  a != b      /// Not equal
```

### Logical

```ez
a and b               /// Logical AND
a or b                /// Logical OR
not a                 /// Logical NOT
```

---

## Control Flow

### If-Else

```ez
when x greater 10 {
    out "x is greater than 10"
}

when x equal 5 {
    out "x is 5"
} when x equal 10 {
    out "x is 10"
} other {
    out "x is something else"
}
```

### While Loop

```ez
until x less 10 {
    out x
    x++
}
```

### For Loop

```ez
/// Counting up
repeat i = 1 to 10 {
    out i
}

/// Counting down
repeat i = 10 to 1 {
    out i
}
```

### Foreach Loop

```ez
numbers = [1, 2, 3, 4, 5]
get num in numbers {
    out num
}
```

### Break and Continue

```ez
repeat i = 1 to 100 {
    when i equal 50 {
        escape            /// Break out of loop
    }
    when i % 2 equal 0 {
        skip              /// Continue to next iteration
    }
    out i
}
```

---

## Functions

### Defining Functions

```ez
task greet(name) {
    out "Hello, " + name
}

task add(a, b) {
    give a + b            /// Return value
}
```

### Calling Functions

```ez
greet("Alice")
result = add(5, 3)
out result                /// 8
```

### Lambda Functions

```ez
square = task(x) {
    give x * x
}

out square(5)             /// 25
```

### Function References

```ez
task double(x) {
    give x * 2
}

f = double               /// Store function reference
out f(10)                /// 20
```

---

## Arrays

### Creating Arrays

```ez
nums = [1, 2, 3, 4, 5]
empty = []
```

### Accessing Elements

```ez
first = nums[0]
last = nums[4]
```

### Modifying Elements

```ez
nums[2] = 100
```

### Array Operations

```ez
/// Add element
nums = add(nums, 6)

/// Remove last element
nums = remove(nums)

/// Get length
size = len(nums)

/// Check if contains
found = contains(nums, 3)

/// Slice array
subset = slice(nums, 1, 4)      /// Elements from index 1 to 3

/// Insert at index
nums = insert(nums, 2, 99)

/// Remove at index
nums = removeAt(nums, 2)

/// Reverse array
reversed = reverse(nums)

/// Sort array
sorted = sort(nums)
```

---

## Built-in Functions

### Math Functions

```ez
/// Basic math
abs(-5)                   /// Absolute value: 5
sqrt(16)                  /// Square root: 4
pow(2, 3)                 /// Power: 8
int(3.7)                  /// Convert to integer: 3

/// Rounding
floor(3.7)                /// Round down: 3
ceil(3.2)                 /// Round up: 4
round(3.5)                /// Round nearest: 4

/// Min/Max
min(5, 3, 8, 1)           /// Minimum: 1
max(5, 3, 8, 1)           /// Maximum: 8

/// Random
random()                  /// Random float 0.0-1.0
randomRange(1, 10)        /// Random integer 1-10
```

### String Functions

```ez
/// Length
len("hello")              /// 5

/// Case conversion
toLowerCase("HELLO")      /// "hello"
toUpperCase("hello")      /// "HELLO"

/// Character access
charAt("hello", 1)        /// "e"

/// Substring
substring("hello", 1, 4)  /// "ell"
substring("hello", 2)     /// "llo"

/// Search
indexOf("hello world", "world")  /// 6
indexOf("hello", "x")            /// -1

/// Split and join
parts = split("a,b,c", ",")      /// ["a", "b", "c"]
joined = join(parts, "-")         /// "a-b-c"

/// Trim whitespace
trim("  hello  ")         /// "hello"

/// Replace
replace("hello", "l", "L") /// "heLLo"

/// Check prefix/suffix
startsWith("hello", "he")  /// yes
endsWith("hello", "lo")    /// yes
```

### Type Functions

```ez
/// Type checking
typeof(42)                /// "number"
typeof("hello")           /// "string"
typeof([1, 2])            /// "array"

isNumber(42)              /// yes
isString("hello")         /// yes
isArray([1, 2])           /// yes
isBool(yes)               /// yes

/// Type conversion
toNumber("42")            /// 42
toStr(42)                 /// "42"
```

### Utility Functions

```ez
/// Time
clock()                   /// Current time in milliseconds
stop(1000)                /// Sleep for 1000ms (1 second)

/// Range generation
range(5)                  /// [0, 1, 2, 3, 4]
range(2, 6)               /// [2, 3, 4, 5]
range(0, 10, 2)           /// [0, 2, 4, 6, 8]
```

---

## File Operations

### Reading Files

```ez
/// Read entire file as string
content = readFile("data.txt")

/// Read file as array of lines
lines = readLines("data.txt")
```

### Writing Files

```ez
/// Write to file (overwrites)
writeFile("output.txt", "Hello, World!")

/// Append to file
appendFile("log.txt", "New entry\n")

/// Write array of lines
lines = ["Line 1", "Line 2", "Line 3"]
writeLines("output.txt", lines)
```

### File Management

```ez
/// Check if file exists
when fileExists("config.txt") {
    out "File exists"
}

/// Delete file
deleted = deleteFile("temp.txt")
```

---

## Web Server

EZ includes built-in web server capabilities (Windows only).

### Basic Server

```ez
/// Define route handler
task home(method, path, query, body) {
    give "<h1>Welcome to EZ Server!</h1>"
}

/// Register route
register_route("/", home)

/// Start server
run_server(8080)
```

### Form Handling

```ez
task handleForm(method, path, query, body) {
    when method equal "POST" {
        /// Parse form data
        fields = parse_form(body)
        
        /// Access form fields
        get field in fields {
            key = field[0]
            value = field[1]
            out key + " = " + value
        }
    }
    
    give "<form method='post'><input name='username'><button>Submit</button></form>"
}

register_route("/form", handleForm)
run_server(8080)
```

### Sessions

```ez
task login(method, path, query, body) {
    /// Start session
    sessionId = session_start()
    
    /// Store data in session
    session_set(sessionId, "username", "Alice")
    
    /// Retrieve session data
    user = session_get(sessionId, "username")
    
    give "Logged in as: " + user
}
```

### Response Helpers

```ez
/// JSON response
data = [["name", "Alice"], ["age", "30"]]
response = json_response(data)

/// Redirect
response = redirect("/home")

/// Custom status
response = abort(404, "Page Not Found")

/// Set cookie
cookie = set_cookie("session", "abc123")

/// Get MIME type
mime = mime_type("file.html")  /// "text/html"
```

### Templates

```ez
/// Replace template variables
html = readFile("template.html")
html = template_replace(html, "username", "Alice")
html = template_replace(html, "title", "Home Page")

/// Include other templates
header = include_template("header.html")
footer = include_template("footer.html")
page = header + content + footer
```

---

## Libraries

### Using Libraries

Create reusable code in separate files:

**math_lib.ez:**
```ez
task factorial(n) {
    when n lessthan 2 {
        give 1
    }
    give n * factorial(n - 1)
}

task fibonacci(n) {
    when n lessthan 2 {
        give n
    }
    give fibonacci(n - 1) + fibonacci(n - 2)
}
```

**main.ez:**
```ez
use "math_lib.ez"          /// Import library

out factorial(5)            /// 120
out fibonacci(10)           /// 55
```

Libraries can also be imported by name without extension:

```ez
use math_lib               /// Same as "math_lib.ez"
```

---

## Example Programs

### Calculator

```ez
task add(a, b) { give a + b }
task subtract(a, b) { give a - b }
task multiply(a, b) { give a * b }
task divide(a, b) {
    when b equal 0 {
        give "Error: Division by zero"
    }
    give a / b
}

out "Enter first number:"
in num1
out "Enter operator (+, -, *, /):"
in op
out "Enter second number:"
in num2

num1 = toNumber(num1)
num2 = toNumber(num2)

when op equal "+" {
    out add(num1, num2)
} when op equal "-" {
    out subtract(num1, num2)
} when op equal "*" {
    out multiply(num1, num2)
} when op equal "/" {
    out divide(num1, num2)
} other {
    out "Invalid operator"
}
```

### Todo List

```ez
todos = []

task showMenu() {
    out "\n=== Todo List ==="
    out "1. Add todo"
    out "2. View todos"
    out "3. Remove todo"
    out "4. Exit"
    out "Choose option:"
}

task addTodo() {
    out "Enter todo:"
    in todo
    todos = add(todos, todo)
    out "Added!"
}

task viewTodos() {
    when len(todos) equal 0 {
        out "No todos yet!"
        give 0
    }
    
    repeat i = 0 to len(todos) - 1 {
        out (i + 1) + ". " + todos[i]
    }
}

task removeTodo() {
    viewTodos()
    out "Enter number to remove:"
    in num
    num = toNumber(num) - 1
    
    when num greaterthan -1 and num less len(todos) {
        todos = removeAt(todos, num)
        out "Removed!"
    } other {
        out "Invalid number"
    }
}

running = yes
until running {
    showMenu()
    in choice
    
    when choice equal "1" {
        addTodo()
    } when choice equal "2" {
        viewTodos()
    } when choice equal "3" {
        removeTodo()
    } when choice equal "4" {
        running = no
    } other {
        out "Invalid choice"
    }
}
```

### File-based Blog

```ez
task savePost(title, content) {
    filename = "posts/" + replace(title, " ", "_") + ".txt"
    data = title + "\n---\n" + content
    writeFile(filename, data)
    out "Post saved!"
}

task listPosts() {
    /// Note: You'd need to implement directory reading
    out "Available posts:"
    /// Implementation depends on directory listing feature
}

task readPost(filename) {
    when fileExists(filename) {
        content = readFile(filename)
        out content
    } other {
        out "Post not found"
    }
}

/// Blog interface
out "1. Create post"
out "2. Read post"
in choice

when choice equal "1" {
    out "Enter title:"
    in title
    out "Enter content:"
    in content
    savePost(title, content)
} when choice equal "2" {
    out "Enter filename:"
    in filename
    readPost(filename)
}
```

### Simple Web Server

```ez
/// Homepage
task home(method, path, query, body) {
    html = "<html><body>"
    html += "<h1>Welcome to EZ Blog</h1>"
    html += "<a href='/create'>Create Post</a> | "
    html += "<a href='/posts'>View Posts</a>"
    html += "</body></html>"
    give html
}

/// Create post form
task createForm(method, path, query, body) {
    when method equal "POST" {
        fields = parse_form(body)
        title = fields[0][1]
        content = fields[1][1]
        
        /// Save post
        savePost(title, content)
        
        give redirect("/posts")
    }
    
    form = "<html><body>"
    form += "<h1>Create Post</h1>"
    form += "<form method='post'>"
    form += "Title: <input name='title'><br>"
    form += "Content: <textarea name='content'></textarea><br>"
    form += "<button>Submit</button>"
    form += "</form></body></html>"
    give form
}

/// Register routes
register_route("/", home)
register_route("/create", createForm)

/// Start server
out "Server starting on http://localhost:8080"
run_server(8080)
```

---

## Tips and Best Practices

1. **Use descriptive variable names**: `userName` is better than `x`

2. **Comment your code**: Use `///` to explain complex logic

3. **Error handling**: Always check for division by zero and array bounds

4. **Modular code**: Break large programs into functions and libraries

5. **Type awareness**: EZ is dynamically typed, but be mindful of conversions

6. **Performance**: Be cautious with deep recursion (max depth: 1000)

7. **File paths**: Use forward slashes `/` even on Windows for portability

---

## Language Features Summary

- **Dynamic typing** with automatic conversions
- **First-class functions** with lambda support
- **Closures** capture surrounding scope
- **Arrays** with comprehensive manipulation functions
- **String operations** with full text processing
- **File I/O** for data persistence
- **Web server** for building web applications
- **Module system** for code organization
- **Math library** with common operations
- **Type introspection** for runtime checks

---

## Limitations

- Maximum recursion depth: 1000
- Web server features are Windows-only
- No multi-threading support
- Arrays are copied on modification (not in-place)
- No hash maps/dictionaries (use arrays of pairs)

---

## Getting Help

For more examples and updates, visit the GitHub repository. Report bugs and suggest features through GitHub Issues.

Happy coding with EZ! 🎉
