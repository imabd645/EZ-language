# EZ Programming Language

**EZ** is a simple, beginner-friendly programming language with modern features including web servers, databases, HTTP requests, package management, and object-oriented programming.

## Table of Contents

- [Features](#features)
- [Installation](#installation)
- [Quick Start](#quick-start)
- [Language Syntax](#language-syntax)
- [Built-in Functions](#built-in-functions)
- [Package Manager](#package-manager)
- [Web Development](#web-development)
- [Database Operations](#database-operations)
- [Object-Oriented Programming](#oop)
- [Examples](#examples)

---

## Features

✨ **Easy to Learn** - Simple, readable syntax  
🌐 **Web Server** - Built-in HTTP server with routing  
🗄️ **SQLite Database** - First-class database support  
📦 **Package Manager** - Install packages from GitHub  
🎨 **OOP Support** - Classes, inheritance, and encapsulation  
🔧 **Rich Standard Library** - File I/O, HTTP requests, JSON parsing  
🎯 **Terminal Control** - Colors, cursor positioning, and more  

---

## Installation

### Prerequisites

- C++ compiler (g++ or MSVC)
- CMake (optional)
- Required libraries:
  - libcurl
  - JsonCpp
  - SQLite3
  - libzip

### Windows Build

```bash
g++ ez.cpp -o ez.exe -lcurl -ljsoncpp -lsqlite3 -lzip -lws2_32
```

### Linux Build

```bash
g++ ez.cpp -o ez -lcurl -ljsoncpp -lsqlite3 -lzip -lpthread
```

---

## Quick Start

### Hello World

```ez
out "Hello, World!"
```

### Variables and Types

```ez
name = "Alice"
age = 25
isStudent = yes
scores = [90, 85, 95]

out "Name:", name
out "Age:", age
```

### Functions

```ez
task greet(name) {
    out "Hello, " + name + "!"
    give "Greeted " + name
}

result = greet("Bob")
out result
```

### Control Flow

```ez
// If-else
when age greater 18 {
    out "Adult"
} other {
    out "Minor"
}

// For loop
repeat i = 1 to 10 {
    out i
}

// While loop
count = 0
until count less 5 {
    out count
    count++
}

// For-each loop
get item in scores {
    out item
}
```

---

## Language Syntax

### Data Types

- **Numbers**: `42`, `3.14`, `-10`
- **Strings**: `"Hello"`, `"World"`
- **Booleans**: `yes`, `no`
- **Arrays**: `[1, 2, 3]`, `["a", "b", "c"]`
- **Structs**: `{name = "Alice", age = 25}`

### Operators

**Arithmetic**: `+`, `-`, `*`, `/`, `//` (integer division), `%`, `**` (power)

**Comparison**: `equal`, `notequal`, `less`, `greater`, `lessthan`, `greaterthan`  
Or symbols: `==`, `!=`, `<`, `>`, `<=`, `>=`

**Logical**: `and`, `or`, `not`

**Assignment**: `=`, `+=`, `-=`, `*=`, `/=`, `%=`

**Increment/Decrement**: `++`, `--`

### Keywords

- **Control Flow**: `when`, `other`, `repeat`, `to`, `until`, `get`, `in`, `escape`, `skip`
- **Functions**: `task`, `give`
- **I/O**: `out`, `in`
- **Modules**: `use`
- **OOP**: `model`, `create`, `from`, `self`, `hidden`, `shown`

### Comments

```ez
// Single-line comment
// This is ignored by the interpreter
```

---

## Built-in Functions

### Array Functions

```ez
arr = [1, 2, 3]
len(arr)              // 3
add(arr, 4)           // [1, 2, 3, 4]
remove(arr)           // [1, 2, 3]
contains(arr, 2)      // yes
slice(arr, 0, 2)      // [1, 2]
range(5)              // [0, 1, 2, 3, 4]
insert(arr, 1, 99)    // [1, 99, 2, 3]
removeAt(arr, 0)      // [2, 3]
reverse(arr)          // [3, 2, 1]
sort(arr)             // [1, 2, 3]
join(arr, ", ")       // "1, 2, 3"
```

### String Functions

```ez
str = "Hello World"
len(str)                    // 11
charAt(str, 0)              // "H"
indexOf(str, "World")       // 6
substring(str, 0, 5)        // "Hello"
split(str, " ")             // ["Hello", "World"]
toLowerCase(str)            // "hello world"
toUpperCase(str)            // "HELLO WORLD"
trim("  hello  ")           // "hello"
replace(str, "World", "EZ") // "Hello EZ"
startsWith(str, "Hello")    // yes
endsWith(str, "World")      // yes
```

### Math Functions

```ez
abs(-5)           // 5
sqrt(16)          // 4
pow(2, 3)         // 8
floor(3.7)        // 3
ceil(3.2)         // 4
round(3.5)        // 4
min(1, 2, 3)      // 1
max(1, 2, 3)      // 3
random()          // 0.0 to 1.0
randomRange(1, 10) // Random integer 1-10
```

### Type Conversion

```ez
int(3.7)          // 3
toNumber("42")    // 42
toStr(42)         // "42"
typeof(42)        // "number"
isNumber(42)      // yes
isString("hi")    // yes
isArray([1,2])    // yes
isBool(yes)       // yes
```

### File I/O

```ez
// Read/write files
content = readFile("data.txt")
writeFile("output.txt", "Hello")
appendFile("log.txt", "New line\n")
fileExists("test.txt")  // yes/no
deleteFile("temp.txt")

// Read/write lines
lines = readLines("data.txt")
writeLines("output.txt", ["line1", "line2"])
```

### Time Functions

```ez
now = clock()     // Current time in milliseconds
stop(1000)        // Sleep for 1 second
```

### Terminal Control (Windows)

```ez
clear()                     // Clear screen
setColor(12)                // Set red text
setColor(14, 0)             // Yellow text, black background
resetColor()                // Reset to default
setCursorPosition(10, 5)    // Move cursor to (10,5)
hideCursor()
showCursor()
size = getConsoleSize()     // [width, height]
setTitle("My App")
```

---

## Package Manager

### Install Packages

```bash
# From GitHub (default: https://github.com/imabd645/EZ{PackageName})
ez install PackageName

# Specific version
ez install PackageName v1.0.0

# Custom repository
ez install mypack main https://github.com/user/repo

# Install from local directory
ez install .
```

### Use Packages in Code

```ez
use "PackageName"

// Use package functions
someFunction()
```

### Other Package Commands

```bash
ez list                  # List installed packages
ez uninstall PackageName # Remove a package
ez search "http"         # Search GitHub for packages
ez update                # Update all packages
ez update PackageName    # Update specific package
ez init myproject        # Create new package
```

### Package Structure (package.ez)

```json
{
  "name": "mypack",
  "version": "1.0.0",
  "description": "My EZ package",
  "author": "Your Name",
  "main": "main.ez",
  "repository": "https://github.com/yourname/mypack",
  "dependencies": {
    "otherpack": "1.0.0"
  }
}
```

---

## Web Development

### Create a Web Server

```ez
task homepage(method, path, query, body) {
    give "<h1>Welcome to EZ!</h1><p>Path: " + path + "</p>"
}

task about(method, path, query, body) {
    give "<h1>About</h1><p>This is an EZ web server</p>"
}

register_route("/", homepage)
register_route("/about", about)

out "Starting server on port 8080..."
run_server(8080)
```

### Form Handling

```ez
task handleForm(method, path, query, body) {
    when method equal "POST" {
        formData = parse_form(body)
        // formData is array of [key, value] pairs
        give "<h1>Form Submitted!</h1>"
    } other {
        give "<form method='POST'>
                <input name='username'>
                <button>Submit</button>
              </form>"
    }
}

register_route("/form", handleForm)
```

### JSON API

```ez
task apiUsers(method, path, query, body) {
    users = [
        ["name", "Alice"],
        ["name", "Bob"]
    ]
    give json_response(users)
}
```

### Sessions

```ez
task login(method, path, query, body) {
    sessId = session_start()
    session_set(sessId, "username", "alice")
    username = session_get(sessId, "username")
    give "Logged in as " + username
}
```

### Redirects and Error Pages

```ez
task oldPage(method, path, query, body) {
    give redirect("/new-page")
}

task notFound(method, path, query, body) {
    give abort(404, "Page not found")
}
```

---

## Database Operations

### Open Database

```ez
db = db_open("mydata.db")
```

### Create Table

```ez
db_execute(db, "CREATE TABLE IF NOT EXISTS users (
    id INTEGER PRIMARY KEY,
    name TEXT,
    email TEXT
)")
```

### Insert Data

```ez
db_execute(db, "INSERT INTO users (name, email) VALUES (?, ?)", 
    "Alice", "alice@example.com")

lastId = db_last_insert_id(db)
out "Inserted user with ID:", lastId
```

### Query Data

```ez
// Get all rows
users = db_query(db, "SELECT * FROM users")
get user in users {
    out user.name, user.email
}

// Get single row
user = db_query_one(db, "SELECT * FROM users WHERE id = ?", 1)
out user.name
```

### Transactions

```ez
db_begin(db)
db_execute(db, "INSERT INTO users (name, email) VALUES (?, ?)", 
    "Bob", "bob@example.com")
db_execute(db, "UPDATE users SET name = ? WHERE id = ?", 
    "Robert", 1)
db_commit(db)

// Or rollback
db_rollback(db)
```

### Database Utilities

```ez
tables = db_tables(db)           // List all tables
exists = db_table_exists(db, "users")  // Check if table exists

// Close database
db_close(db)
```

---

## Object-Oriented Programming

### Define a Class

```ez
model Person {
    name = ""
    age = 0
    
    // Constructor
    task init(n, a) {
        self.name = n
        self.age = a
    }
    
    // Method
    task greet() {
        out "Hello, I'm " + self.name
        give "Greeting from " + self.name
    }
    
    task birthday() {
        self.age = self.age + 1
        out self.name + " is now " + toStr(self.age)
    }
}
```

### Create Objects

```ez
person1 = create Person("Alice", 25)
person1.greet()          // "Hello, I'm Alice"
person1.birthday()       // "Alice is now 26"

out person1.name         // "Alice"
out person1.age          // 26
```

### Inheritance

```ez
model Student from Person {
    grade = "A"
    
    task init(n, a, g) {
        self.name = n
        self.age = a
        self.grade = g
    }
    
    task study() {
        out self.name + " is studying..."
    }
}

student = create Student("Bob", 20, "A+")
student.greet()    // Inherited method
student.study()    // New method
```

### Access Modifiers

```ez
model BankAccount {
    shown balance = 1000
    hidden pin = "1234"
    
    shown task withdraw(amount) {
        self.balance = self.balance - amount
    }
    
    hidden task checkPin(inputPin) {
        give inputPin equal self.pin
    }
}

account = create BankAccount()
out account.balance  // OK - public
// out account.pin   // Error - private field
```

### OOP Utility Functions

```ez
typeof(person1)              // "object:Person"
isObject(person1)            // yes
classOf(person1)             // "Person"
isInstance(student, "Student")  // yes
isInstance(student, "Person")   // yes (inheritance)
```

---

## HTTP Requests

### GET Request

```ez
response = http_get("https://api.example.com/data")
out response

// With headers
headers = {Authorization = "Bearer token123"}
response = http_get("https://api.example.com/data", headers)
```

### POST Request

```ez
data = '{"name": "Alice", "email": "alice@example.com"}'
headers = {Content-Type = "application/json"}
response = http_post("https://api.example.com/users", data, headers)
```

### Other HTTP Methods

```ez
http_put("https://api.example.com/users/1", data, headers)
http_delete("https://api.example.com/users/1")
```

### JSON Parsing

```ez
jsonStr = '{"name": "Alice", "age": 25}'
data = parse_json(jsonStr)
out data.name  // "Alice"
out data.age   // 25

// Convert back to JSON
newJson = to_json(data)
```

### URL Encoding

```ez
encoded = url_encode("hello world")  // "hello%20world"
decoded = url_decode(encoded)        // "hello world"
```

---

## Examples

### Todo List Manager

```ez
db = db_open("todos.db")
db_execute(db, "CREATE TABLE IF NOT EXISTS todos (
    id INTEGER PRIMARY KEY,
    task TEXT,
    done INTEGER DEFAULT 0
)")

task addTodo(taskName) {
    db_execute(db, "INSERT INTO todos (task) VALUES (?)", taskName)
    out "Added:", taskName
}

task listTodos() {
    todos = db_query(db, "SELECT * FROM todos")
    get todo in todos {
        status = "[ ]"
        when todo.done equal 1 {
            status = "[X]"
        }
        out status, todo.task
    }
}

task completeTodo(id) {
    db_execute(db, "UPDATE todos SET done = 1 WHERE id = ?", id)
    out "Completed task", id
}

addTodo("Learn EZ")
addTodo("Build a project")
listTodos()
completeTodo(1)
listTodos()

db_close(db)
```

### Weather API

```ez
task getWeather(city) {
    url = "https://api.openweathermap.org/data/2.5/weather?q=" + 
          url_encode(city) + "&appid=YOUR_API_KEY"
    
    response = http_get(url)
    data = parse_json(response)
    
    out "Weather in", city
    out "Temperature:", data.main.temp
    out "Description:", data.weather[0].description
}

getWeather("London")
```

### Simple Blog

```ez
model Post {
    title = ""
    content = ""
    author = ""
    
    task init(t, c, a) {
        self.title = t
        self.content = c
        self.author = a
    }
    
    task display() {
        out "=== " + self.title + " ==="
        out "By:", self.author
        out self.content
        out ""
    }
}

posts = []
posts = add(posts, create Post("Hello World", "My first post", "Alice"))
posts = add(posts, create Post("EZ is Great", "I love this language", "Bob"))

get post in posts {
    post.display()
}
```

### File Backup System

```ez
task backupFiles(sourceDir, backupDir) {
    timestamp = toStr(clock())
    backupPath = backupDir + "/backup_" + timestamp + ".txt"
    
    files = readLines(sourceDir + "/filelist.txt")
    
    allContent = ""
    get filename in files {
        when fileExists(sourceDir + "/" + filename) {
            content = readFile(sourceDir + "/" + filename)
            allContent = allContent + "\n=== " + filename + " ===\n" + content
        }
    }
    
    writeFile(backupPath, allContent)
    out "Backup created:", backupPath
}

backupFiles("./src", "./backups")
```

---

## Advanced Features

### Structs

```ez
person = {
    name = "Alice",
    age = 25,
    city = "New York"
}

out person.name
person.age = 26

// Struct functions
keys = keys(person)        // ["name", "age", "city"]
values = values(person)    // ["Alice", 26, "New York"]
hasField(person, "name")   // yes
```

### Lambda Functions

```ez
add = task(a, b) {
    give a + b
}

result = add(5, 3)  // 8

// Higher-order functions
task apply(func, x, y) {
    give func(x, y)
}

result = apply(add, 10, 20)  // 30
```

### Module System

```ez
// mathlib.ez
task square(x) {
    give x * x
}

task cube(x) {
    give x * x * x
}
```

```ez
// main.ez
use "mathlib"

out square(5)   // 25
out cube(3)     // 27
```

---

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.

## License

This project is open source and available under the MIT License.

## Links

- GitHub Repository: [https://github.com/imabd645/ez-language](https://github.com/imabd645/ez-language)

---

Made with ❤️ by the Abdullah Masood
